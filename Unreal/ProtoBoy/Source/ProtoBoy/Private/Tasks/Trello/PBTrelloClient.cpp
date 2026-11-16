#include "Tasks/Trello/PBTrelloClient.h"

#include "Dom/JsonObject.h"
#include "Net/PBHttpClient.h"
#include "PBSettings.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Tasks/Trello/PBTrelloConnector.h"
#include "Tasks/Trello/PBTrelloJson.h"

#define LOCTEXT_NAMESPACE "ProtoBoyTrelloClient"

FProtoBoyTrelloClient::FProtoBoyTrelloClient(const FProtoBoyTrelloConnector& InConnector)
	: Connector(InConnector)
{
}

bool FProtoBoyTrelloClient::IsSafeId(const FString& Id)
{
	if (Id.IsEmpty() || Id.Len() > 64)
	{
		return false;
	}
	for (const TCHAR C : Id)
	{
		if (!FChar::IsAlnum(C))
		{
			return false;
		}
	}
	return true;
}

void FProtoBoyTrelloClient::Call(const FString& Verb, const FString& Path, const TSharedPtr<FJsonObject>& Body, FJsonCallback OnDone) const
{
	CallAttempt(Verb, Path, ProtoBoyTrelloClient::ToJson(Body), MoveTemp(OnDone), 0);
}

void FProtoBoyTrelloClient::CallAttempt(const FString& Verb, const FString& Path, const FString& Body, FJsonCallback OnDone, int32 Attempt) const
{
	const FProtoBoyTasksConnection& Connection = Connector.GetConnection();
	if (!Connector.IsConnected())
	{
		OnDone(false, LOCTEXT("NotConnected", "Trello isn't connected. Connect it in ProtoBoy > Connect Task Board."), nullptr);
		return;
	}

	TMap<FString, FString> Headers;
	Headers.Add(TEXT("Authorization"), FString::Printf(TEXT("OAuth oauth_consumer_key=\"%s\", oauth_token=\"%s\""),
		*Connection.TrelloApiKey, *Connection.TrelloToken));

	ProtoBoyHttp::SendRequest(Verb, FString(ProtoBoyTrelloClient::ApiRoot) + Path, Headers, Body,
		[this, Verb, Path, Body, Attempt, OnDone = MoveTemp(OnDone)](const ProtoBoyHttp::FResponse& Response) mutable
		{
			if (!Response.bReachedServer)
			{
				OnDone(false, LOCTEXT("Offline", "Couldn't reach api.trello.com."), nullptr);
				return;
			}
			if (Response.Code == 429)
			{
				// Rate limited: Trello didn't act on it, so it's safe to send again (any verb).
				if (Attempt >= MaxRetries)
				{
					OnDone(false, LOCTEXT("RateLimited", "Trello is limiting how fast ProtoBoy can make changes. Wait a minute and try again."), nullptr);
					return;
				}
				const float Delay = Response.RetryAfterSeconds > 0.f ? Response.RetryAfterSeconds : FMath::Min(16.f, FMath::Pow(2.f, static_cast<float>(Attempt)));
				ProtoBoyHttp::RunAfter(Delay, [this, Verb, Path, Body, Attempt, OnDone = MoveTemp(OnDone)]() mutable
				{
					CallAttempt(Verb, Path, Body, MoveTemp(OnDone), Attempt + 1);
				});
				return;
			}
			if (Response.Code == 401)
			{
				OnDone(false, LOCTEXT("Unauthorized", "Trello rejected ProtoBoy's token. Reconnect Trello in ProtoBoy."), nullptr);
				return;
			}
			if (Response.Code == 404)
			{
				OnDone(false, LOCTEXT("NotFound", "Trello couldn't find that."), nullptr);
				return;
			}
			if (!Response.IsOk())
			{
				OnDone(false, FText::Format(LOCTEXT("HttpError", "Trello answered with HTTP {0}: {1}"),
					FText::AsNumber(Response.Code), FText::FromString(Response.Body.Left(200))), nullptr);
				return;
			}

			TSharedPtr<FJsonValue> Json;
			if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response.Body), Json) || !Json.IsValid())
			{
				OnDone(false, LOCTEXT("BadJson", "Trello sent an unexpected reply."), nullptr);
				return;
			}
			OnDone(true, FText::GetEmpty(), Json);
		});
}

FText FProtoBoyTrelloClient::GetDisplayName() const
{
	const FString& Board = GetDefault<UProtoBoySettings>()->TrelloBoardName;
	return Board.IsEmpty() ? LOCTEXT("Name", "Trello") : FText::Format(LOCTEXT("NameBoard", "Trello (board \"{0}\")"), FText::FromString(Board));
}

bool FProtoBoyTrelloClient::IsReady(FText& OutWhyNot) const
{
	const UProtoBoySettings* Settings = GetDefault<UProtoBoySettings>();
	if (!Connector.IsConnected())
	{
		OutWhyNot = LOCTEXT("NotReadyConnect", "Trello isn't connected for this user.");
		return false;
	}
	if (Settings->TrelloBoardId.IsEmpty())
	{
		OutWhyNot = LOCTEXT("NotReadyBoard", "No Trello board is chosen for this project.");
		return false;
	}
	if (Settings->TrelloToDoListId.IsEmpty() || Settings->TrelloInProgressListId.IsEmpty() || Settings->TrelloDoneListId.IsEmpty())
	{
		OutWhyNot = LOCTEXT("NotReadyLists", "The Trello board's To Do / In Progress / Done lists aren't mapped yet.");
		return false;
	}
	return true;
}

FString FProtoBoyTrelloClient::ListIdFor(EProtoBoyTaskStatus Status) const
{
	const UProtoBoySettings* Settings = GetDefault<UProtoBoySettings>();
	switch (Status)
	{
	case EProtoBoyTaskStatus::ToDo: return Settings->TrelloToDoListId;
	case EProtoBoyTaskStatus::InProgress: return Settings->TrelloInProgressListId;
	case EProtoBoyTaskStatus::Done: return Settings->TrelloDoneListId;
	case EProtoBoyTaskStatus::Other: break;
	}
	return FString();
}

EProtoBoyTaskStatus FProtoBoyTrelloClient::StatusForList(const FString& ListId) const
{
	const UProtoBoySettings* Settings = GetDefault<UProtoBoySettings>();
	if (ListId == Settings->TrelloToDoListId) { return EProtoBoyTaskStatus::ToDo; }
	if (ListId == Settings->TrelloInProgressListId) { return EProtoBoyTaskStatus::InProgress; }
	if (ListId == Settings->TrelloDoneListId) { return EProtoBoyTaskStatus::Done; }
	return EProtoBoyTaskStatus::Other;
}

FProtoBoyTask FProtoBoyTrelloClient::ParseCard(const TSharedPtr<FJsonObject>& Card, const TMap<FString, FString>& ListNames) const
{
	using ProtoBoyTrelloClient::GetString;

	FProtoBoyTask Task;
	const FString ShortLink = GetString(Card, TEXT("shortLink"));
	Task.Id = ShortLink.IsEmpty() ? GetString(Card, TEXT("id")) : ShortLink;
	Task.Title = GetString(Card, TEXT("name"));
	Task.Description = GetString(Card, TEXT("desc"));
	Task.Url = GetString(Card, TEXT("shortUrl"));
	Task.DueUtc = GetString(Card, TEXT("due"));
	Task.LastActivityUtc = GetString(Card, TEXT("dateLastActivity"));
	Task.DesignDoc = ProtoBoyTasks::FindDesignDoc(Task.Description);

	const FString ListId = GetString(Card, TEXT("idList"));
	Task.Status = StatusForList(ListId);
	if (const FString* ListName = ListNames.Find(ListId))
	{
		Task.StatusName = *ListName;
	}

	const TArray<TSharedPtr<FJsonValue>>* Labels = nullptr;
	if (Card->TryGetArrayField(TEXT("labels"), Labels))
	{
		for (const TSharedPtr<FJsonValue>& Label : *Labels)
		{
			const TSharedPtr<FJsonObject>* LabelObject = nullptr;
			if (Label->TryGetObject(LabelObject))
			{
				const FString Name = GetString(*LabelObject, TEXT("name"));
				if (!Name.IsEmpty())
				{
					Task.Labels.Add(Name);
				}
			}
		}
	}
	return Task;
}

void FProtoBoyTrelloClient::ListTasks(TOptional<EProtoBoyTaskStatus> Filter, FTasksCallback OnDone)
{
	FText WhyNot;
	if (!IsReady(WhyNot))
	{
		OnDone(TProtoBoyTaskResult<TArray<FProtoBoyTask>>::Failure(WhyNot));
		return;
	}

	// One request: the board's open lists (for names) and open cards.
	const FString Path = FString::Printf(TEXT("boards/%s?fields=name&lists=open&list_fields=name&cards=open&card_%s"),
		*GetDefault<UProtoBoySettings>()->TrelloBoardId, ProtoBoyTrelloClient::CardFields);

	Call(TEXT("GET"), Path, nullptr, [this, Filter, OnDone = MoveTemp(OnDone)](bool bOk, const FText& Error, const TSharedPtr<FJsonValue>& Json)
	{
		const TSharedPtr<FJsonObject>* Board = nullptr;
		if (!bOk || !Json->TryGetObject(Board))
		{
			OnDone(TProtoBoyTaskResult<TArray<FProtoBoyTask>>::Failure(bOk ? LOCTEXT("BadBoardReply", "Trello sent an unexpected board.") : Error));
			return;
		}

		TMap<FString, FString> ListNames;
		const TArray<TSharedPtr<FJsonValue>>* Lists = nullptr;
		if ((*Board)->TryGetArrayField(TEXT("lists"), Lists))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Lists)
			{
				const TSharedPtr<FJsonObject>* List = nullptr;
				if (Value->TryGetObject(List))
				{
					ListNames.Add(ProtoBoyTrelloClient::GetString(*List, TEXT("id")), ProtoBoyTrelloClient::GetString(*List, TEXT("name")));
				}
			}
		}

		TArray<FProtoBoyTask> Tasks;
		const TArray<TSharedPtr<FJsonValue>>* Cards = nullptr;
		if ((*Board)->TryGetArrayField(TEXT("cards"), Cards))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Cards)
			{
				const TSharedPtr<FJsonObject>* Card = nullptr;
				if (Value->TryGetObject(Card))
				{
					FProtoBoyTask Task = ParseCard(*Card, ListNames);
					if (!Filter.IsSet() || Task.Status == Filter.GetValue())
					{
						Tasks.Add(MoveTemp(Task));
					}
				}
			}
		}
		OnDone(TProtoBoyTaskResult<TArray<FProtoBoyTask>>::Success(MoveTemp(Tasks)));
	});
}

void FProtoBoyTrelloClient::GetCardOnBoard(const FString& Id, const FString& ExtraQuery, FJsonCallback OnDone) const
{
	FText WhyNot;
	if (!IsReady(WhyNot))
	{
		OnDone(false, WhyNot, nullptr);
		return;
	}
	if (!IsSafeId(Id))
	{
		OnDone(false, LOCTEXT("BadId", "Task ids are Trello card ids or short links (letters and digits)."), nullptr);
		return;
	}

	const FString Path = FString::Printf(TEXT("cards/%s?%s&list=true&list_fields=name%s"), *Id, ProtoBoyTrelloClient::CardFields, *ExtraQuery);
	Call(TEXT("GET"), Path, nullptr, [OnDone = MoveTemp(OnDone)](bool bOk, const FText& Error, const TSharedPtr<FJsonValue>& Json)
	{
		const TSharedPtr<FJsonObject>* Card = nullptr;
		if (!bOk || !Json->TryGetObject(Card))
		{
			OnDone(false, bOk ? LOCTEXT("BadCard", "Trello sent an unexpected card.") : Error, nullptr);
			return;
		}
		// Agents only get the team's board, even if the user's token can see others.
		if (ProtoBoyTrelloClient::GetString(*Card, TEXT("idBoard")) != GetDefault<UProtoBoySettings>()->TrelloBoardId)
		{
			OnDone(false, LOCTEXT("OtherBoard", "That card isn't on this project's Trello board."), nullptr);
			return;
		}
		OnDone(true, FText::GetEmpty(), Json);
	});
}

void FProtoBoyTrelloClient::GetTask(const FString& Id, FTaskCallback OnDone)
{
	GetCardOnBoard(Id, TEXT("&actions=commentCard&actions_limit=20"), [this, OnDone = MoveTemp(OnDone)](bool bOk, const FText& Error, const TSharedPtr<FJsonValue>& Json)
	{
		if (!bOk)
		{
			OnDone(TProtoBoyTaskResult<FProtoBoyTask>::Failure(Error));
			return;
		}

		const TSharedPtr<FJsonObject> Card = Json->AsObject();
		TMap<FString, FString> ListNames;
		const TSharedPtr<FJsonObject>* List = nullptr;
		if (Card->TryGetObjectField(TEXT("list"), List))
		{
			ListNames.Add(ProtoBoyTrelloClient::GetString(Card, TEXT("idList")), ProtoBoyTrelloClient::GetString(*List, TEXT("name")));
		}

		FProtoBoyTask Task = ParseCard(Card, ListNames);

		const TArray<TSharedPtr<FJsonValue>>* Actions = nullptr;
		if (Card->TryGetArrayField(TEXT("actions"), Actions))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Actions)
			{
				const TSharedPtr<FJsonObject>* Action = nullptr;
				const TSharedPtr<FJsonObject>* Data = nullptr;
				if (!Value->TryGetObject(Action) || !(*Action)->TryGetObjectField(TEXT("data"), Data))
				{
					continue;
				}
				FProtoBoyTaskComment Comment;
				Comment.Text = ProtoBoyTrelloClient::GetString(*Data, TEXT("text"));
				Comment.DateUtc = ProtoBoyTrelloClient::GetString(*Action, TEXT("date"));
				const TSharedPtr<FJsonObject>* Member = nullptr;
				if ((*Action)->TryGetObjectField(TEXT("memberCreator"), Member))
				{
					Comment.Author = ProtoBoyTrelloClient::GetString(*Member, TEXT("username"));
				}
				Task.Comments.Add(MoveTemp(Comment));
			}
		}
		OnDone(TProtoBoyTaskResult<FProtoBoyTask>::Success(MoveTemp(Task)));
	});
}

void FProtoBoyTrelloClient::CreateTask(const FString& Title, const FString& Description, EProtoBoyTaskStatus Status, FTaskCallback OnDone)
{
	FText WhyNot;
	if (!IsReady(WhyNot))
	{
		OnDone(TProtoBoyTaskResult<FProtoBoyTask>::Failure(WhyNot));
		return;
	}
	const FString ListId = ListIdFor(Status == EProtoBoyTaskStatus::Other ? EProtoBoyTaskStatus::ToDo : Status);

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("idList"), ListId);
	Body->SetStringField(TEXT("name"), Title.Left(16384));
	Body->SetStringField(TEXT("desc"), Description.Left(16384));
	Body->SetStringField(TEXT("pos"), TEXT("bottom"));

	Call(TEXT("POST"), TEXT("cards"), Body, [this, OnDone = MoveTemp(OnDone)](bool bOk, const FText& Error, const TSharedPtr<FJsonValue>& Json)
	{
		const TSharedPtr<FJsonObject>* Card = nullptr;
		if (!bOk || !Json->TryGetObject(Card))
		{
			OnDone(TProtoBoyTaskResult<FProtoBoyTask>::Failure(bOk ? LOCTEXT("BadCreate", "Trello sent an unexpected card.") : Error));
			return;
		}
		OnDone(TProtoBoyTaskResult<FProtoBoyTask>::Success(ParseCard(*Card, {})));
	});
}

void FProtoBoyTrelloClient::SetTaskStatus(const FString& Id, EProtoBoyTaskStatus Status, FTaskCallback OnDone)
{
	const FString ListId = ListIdFor(Status);
	if (ListId.IsEmpty())
	{
		OnDone(TProtoBoyTaskResult<FProtoBoyTask>::Failure(LOCTEXT("BadStatus", "Status must be todo, in_progress or done.")));
		return;
	}

	GetCardOnBoard(Id, FString(), [this, ListId, OnDone = MoveTemp(OnDone)](bool bOk, const FText& Error, const TSharedPtr<FJsonValue>& Json) mutable
	{
		if (!bOk)
		{
			OnDone(TProtoBoyTaskResult<FProtoBoyTask>::Failure(Error));
			return;
		}

		const FString CardId = ProtoBoyTrelloClient::GetString(Json->AsObject(), TEXT("id"));
		TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
		Body->SetStringField(TEXT("idList"), ListId);

		Call(TEXT("PUT"), FString::Printf(TEXT("cards/%s"), *CardId), Body,
			[this, OnDone = MoveTemp(OnDone)](bool bPutOk, const FText& PutError, const TSharedPtr<FJsonValue>& Updated)
			{
				const TSharedPtr<FJsonObject>* Card = nullptr;
				if (!bPutOk || !Updated->TryGetObject(Card))
				{
					OnDone(TProtoBoyTaskResult<FProtoBoyTask>::Failure(bPutOk ? LOCTEXT("BadUpdate", "Trello sent an unexpected card.") : PutError));
					return;
				}
				OnDone(TProtoBoyTaskResult<FProtoBoyTask>::Success(ParseCard(*Card, {})));
			});
	});
}

void FProtoBoyTrelloClient::AddComment(const FString& Id, const FString& Text, FDoneCallback OnDone)
{
	if (Text.TrimStartAndEnd().IsEmpty())
	{
		OnDone(TProtoBoyTaskResult<bool>::Failure(LOCTEXT("EmptyComment", "The comment is empty.")));
		return;
	}

	GetCardOnBoard(Id, FString(), [this, Text, OnDone = MoveTemp(OnDone)](bool bOk, const FText& Error, const TSharedPtr<FJsonValue>& Json) mutable
	{
		if (!bOk)
		{
			OnDone(TProtoBoyTaskResult<bool>::Failure(Error));
			return;
		}

		const FString CardId = ProtoBoyTrelloClient::GetString(Json->AsObject(), TEXT("id"));
		TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
		Body->SetStringField(TEXT("text"), Text.Left(16384));

		Call(TEXT("POST"), FString::Printf(TEXT("cards/%s/actions/comments"), *CardId), Body,
			[OnDone = MoveTemp(OnDone)](bool bPostOk, const FText& PostError, const TSharedPtr<FJsonValue>&)
			{
				OnDone(bPostOk ? TProtoBoyTaskResult<bool>::Success(true) : TProtoBoyTaskResult<bool>::Failure(PostError));
			});
	});
}

void FProtoBoyTrelloClient::CreateTasks(const TArray<FProtoBoyNewTask>& Tasks, FEachTaskCallback OnEach, FDoneCallback OnDone)
{
	using FResult = TProtoBoyTaskResult<bool>;
	FText WhyNot;
	if (!IsReady(WhyNot))
	{
		OnDone(FResult::Failure(WhyNot));
		return;
	}
	const UProtoBoySettings* Settings = GetDefault<UProtoBoySettings>();
	const FString BoardId = Settings->TrelloBoardId;
	if (!IsSafeId(BoardId))
	{
		OnDone(FResult::Failure(LOCTEXT("CreateBadBoard", "Invalid board id.")));
		return;
	}

	struct FState
	{
		TArray<FProtoBoyNewTask> Tasks;
		int32 Next = 0;
		int32 NumCreated = 0;
		FText LastError;
		/** The board's lists, its labels by name (lowercase) -> id, and every label id. */
		TArray<FProtoBoyTrelloItem> Lists;
		TMap<FString, FString> Labels;
		TSet<FString> LabelIds;
		FEachTaskCallback OnEach;
		FDoneCallback OnDone;
		TFunction<void()> CreateNext;
	};
	const TSharedRef<FState> State = MakeShared<FState>();
	State->Tasks = Tasks;
	State->OnEach = MoveTemp(OnEach);
	State->OnDone = MoveTemp(OnDone);
	const TWeakPtr<FState> WeakState = State;

	// Step 3: one card at a time, in order.
	State->CreateNext = [this, WeakState]()
	{
		const TSharedPtr<FState> Pinned = WeakState.Pin();
		if (!Pinned.IsValid())
		{
			return;
		}
		if (!Pinned->Tasks.IsValidIndex(Pinned->Next))
		{
			FDoneCallback Callback = MoveTemp(Pinned->OnDone);
			Callback(Pinned->NumCreated > 0 || Pinned->Tasks.IsEmpty() ? FResult::Success(true) : FResult::Failure(Pinned->LastError));
			return;
		}
		const int32 Index = Pinned->Next++;
		const FProtoBoyNewTask& Task = Pinned->Tasks[Index];

		// The category's own list (remembered by Sync, else by name), else Uncategorized, else the plain To Do list.
		const UProtoBoySettings* Settings = GetDefault<UProtoBoySettings>();
		const FString Category = Task.Category.TrimStartAndEnd();
		const bool bHasCategory = !Task.CategoryId.IsEmpty() || (!Category.IsEmpty() && !ProtoBoyTasks::IsReservedCategoryName(Category));
		auto HasList = [&Pinned](const FString& Id) { return !Id.IsEmpty() && Pinned->Lists.ContainsByPredicate([&Id](const FProtoBoyTrelloItem& L) { return L.Id == Id; }); };
		auto FindListByName = [&Pinned](const FString& Wanted) -> FString
		{
			for (const FProtoBoyTrelloItem& List : Pinned->Lists)
			{
				FString ListCategory;
				if (ParseCategoryListName(List.Name, ListCategory) && ListCategory.Equals(Wanted, ESearchCase::IgnoreCase))
				{
					return List.Id;
				}
			}
			return FString();
		};
		FString ListId;
		FString LabelId;
		if (bHasCategory)
		{
			ListId = Settings->TrelloCategoryListIds.FindRef(Task.CategoryId);
			if (!HasList(ListId))
			{
				ListId = Category.IsEmpty() ? FString() : FindListByName(Category);
			}
			LabelId = Settings->TrelloCategoryLabelIds.FindRef(Task.CategoryId);
			if (LabelId.IsEmpty() || !Pinned->LabelIds.Contains(LabelId))
			{
				LabelId = Category.IsEmpty() ? FString() : Pinned->Labels.FindRef(Category.ToLower());
			}
		}
		if (ListId.IsEmpty())
		{
			// Uncategorized carries no category label.
			LabelId.Reset();
			ListId = FindListByName(ProtoBoyTasks::UncategorizedName);
		}
		if (ListId.IsEmpty())
		{
			ListId = Settings->TrelloToDoListId;
		}

		TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
		Body->SetStringField(TEXT("name"), Task.Title.Left(16384));
		Body->SetStringField(TEXT("desc"), Task.Description.Left(16384));
		Body->SetStringField(TEXT("idList"), ListId);
		Body->SetStringField(TEXT("pos"), TEXT("bottom"));
		if (!LabelId.IsEmpty())
		{
			Body->SetStringField(TEXT("idLabels"), LabelId);
		}

		const TSharedRef<FState> Strong = Pinned.ToSharedRef();
		Call(TEXT("POST"), FString::Printf(TEXT("cards?%s"), ProtoBoyTrelloClient::CardFields), Body, [this, Strong, Index](bool bOk, const FText& Error, const TSharedPtr<FJsonValue>& Json)
		{
			const TSharedPtr<FJsonObject>* Card = nullptr;
			bool bContinue = true;
			if (bOk && Json->TryGetObject(Card))
			{
				TMap<FString, FString> ListNames;
				for (const FProtoBoyTrelloItem& List : Strong->Lists)
				{
					ListNames.Add(List.Id, List.Name);
				}
				++Strong->NumCreated;
				bContinue = Strong->OnEach(Index, TProtoBoyTaskResult<FProtoBoyTask>::Success(ParseCard(*Card, ListNames)));
			}
			else
			{
				Strong->LastError = bOk ? LOCTEXT("CreateBadCard", "Trello sent an unexpected card.") : Error;
				bContinue = Strong->OnEach(Index, TProtoBoyTaskResult<FProtoBoyTask>::Failure(Strong->LastError));
			}
			if (!bContinue)
			{
				// Stopped by the caller: the rest aren't created.
				Strong->Tasks.SetNum(Strong->Next);
			}
			Strong->CreateNext();
		});
	};

	// Step 2: the board's labels.
	auto FetchLabels = [this, BoardId, State]()
	{
		Call(TEXT("GET"), FString::Printf(TEXT("boards/%s/labels?fields=name&limit=1000"), *BoardId), nullptr,
			[State](bool bOk, const FText& Error, const TSharedPtr<FJsonValue>& Json)
			{
				const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
				if (!bOk || !Json->TryGetArray(Array))
				{
					FDoneCallback Callback = MoveTemp(State->OnDone);
					Callback(FResult::Failure(bOk ? LOCTEXT("CreateBadLabels", "Trello sent an unexpected list of labels.") : Error));
					return;
				}
				for (const TSharedPtr<FJsonValue>& Value : *Array)
				{
					const TSharedPtr<FJsonObject>* Label = nullptr;
					if (Value->TryGetObject(Label))
					{
						State->LabelIds.Add(ProtoBoyTrelloClient::GetString(*Label, TEXT("id")));
						const FString Name = ProtoBoyTrelloClient::GetString(*Label, TEXT("name")).TrimStartAndEnd().ToLower();
						if (!Name.IsEmpty() && !State->Labels.Contains(Name))
						{
							State->Labels.Add(Name, ProtoBoyTrelloClient::GetString(*Label, TEXT("id")));
						}
					}
				}
				State->CreateNext();
			});
	};

	// Step 1: the board's lists.
	FetchLists(BoardId, [State, FetchLabels](const TProtoBoyTaskResult<TArray<FProtoBoyTrelloItem>>& Result)
	{
		if (!Result.bOk)
		{
			FDoneCallback Callback = MoveTemp(State->OnDone);
			Callback(FResult::Failure(Result.Error));
			return;
		}
		State->Lists = Result.Value;
		FetchLabels();
	});
}

#undef LOCTEXT_NAMESPACE
