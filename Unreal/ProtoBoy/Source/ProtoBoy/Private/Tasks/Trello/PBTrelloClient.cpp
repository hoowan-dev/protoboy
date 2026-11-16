#include "Tasks/Trello/PBTrelloClient.h"

#include "Docs/PBDocGenerator.h"
#include "Dom/JsonObject.h"
#include "Misc/App.h"
#include "Net/PBHttpClient.h"
#include "PBSettings.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Tasks/Trello/PBTrelloConnector.h"

#define LOCTEXT_NAMESPACE "ProtoBoyTrelloClient"

namespace ProtoBoyTrelloClient
{
	const TCHAR* ApiRoot = TEXT("https://api.trello.com/1/");
	const TCHAR* CardFields = TEXT("fields=name,desc,idList,idBoard,shortLink,shortUrl,labels,due,dateLastActivity");

	FString ToJson(const TSharedPtr<FJsonObject>& Object)
	{
		FString Out;
		if (Object.IsValid())
		{
			FJsonSerializer::Serialize(Object.ToSharedRef(), TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out));
		}
		return Out;
	}

	FString GetString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		FString Value;
		if (Object.IsValid())
		{
			Object->TryGetStringField(Field, Value);
		}
		return Value;
	}
}

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
	const FProtoBoyTasksConnection& Connection = Connector.GetConnection();
	if (!Connector.IsConnected())
	{
		OnDone(false, LOCTEXT("NotConnected", "Trello isn't connected. Connect it in ProtoBoy > Connect Task Board."), nullptr);
		return;
	}

	TMap<FString, FString> Headers;
	Headers.Add(TEXT("Authorization"), FString::Printf(TEXT("OAuth oauth_consumer_key=\"%s\", oauth_token=\"%s\""),
		*Connection.TrelloApiKey, *Connection.TrelloToken));

	ProtoBoyHttp::SendRequest(Verb, FString(ProtoBoyTrelloClient::ApiRoot) + Path, Headers, ProtoBoyTrelloClient::ToJson(Body),
		[OnDone = MoveTemp(OnDone)](const ProtoBoyHttp::FResponse& Response)
		{
			if (!Response.bReachedServer)
			{
				OnDone(false, LOCTEXT("Offline", "Couldn't reach api.trello.com."), nullptr);
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

void FProtoBoyTrelloClient::FetchBoards(FItemsCallback OnDone)
{
	Call(TEXT("GET"), TEXT("members/me/boards?filter=open&fields=name"), nullptr,
		[OnDone = MoveTemp(OnDone)](bool bOk, const FText& Error, const TSharedPtr<FJsonValue>& Json)
		{
			if (!bOk)
			{
				OnDone(TProtoBoyTaskResult<TArray<FProtoBoyTrelloItem>>::Failure(Error));
				return;
			}
			TArray<FProtoBoyTrelloItem> Items;
			const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
			if (Json->TryGetArray(Array))
			{
				for (const TSharedPtr<FJsonValue>& Value : *Array)
				{
					const TSharedPtr<FJsonObject>* Board = nullptr;
					if (Value->TryGetObject(Board))
					{
						Items.Add({ ProtoBoyTrelloClient::GetString(*Board, TEXT("id")), ProtoBoyTrelloClient::GetString(*Board, TEXT("name")) });
					}
				}
			}
			Items.Sort([](const FProtoBoyTrelloItem& A, const FProtoBoyTrelloItem& B) { return A.Name < B.Name; });
			OnDone(TProtoBoyTaskResult<TArray<FProtoBoyTrelloItem>>::Success(MoveTemp(Items)));
		});
}

void FProtoBoyTrelloClient::FetchLists(const FString& BoardId, FItemsCallback OnDone)
{
	if (!IsSafeId(BoardId))
	{
		OnDone(TProtoBoyTaskResult<TArray<FProtoBoyTrelloItem>>::Failure(LOCTEXT("BadBoard", "Invalid board id.")));
		return;
	}

	Call(TEXT("GET"), FString::Printf(TEXT("boards/%s/lists?filter=open&fields=name"), *BoardId), nullptr,
		[OnDone = MoveTemp(OnDone)](bool bOk, const FText& Error, const TSharedPtr<FJsonValue>& Json)
		{
			if (!bOk)
			{
				OnDone(TProtoBoyTaskResult<TArray<FProtoBoyTrelloItem>>::Failure(Error));
				return;
			}
			TArray<FProtoBoyTrelloItem> Items;
			const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
			if (Json->TryGetArray(Array))
			{
				for (const TSharedPtr<FJsonValue>& Value : *Array)
				{
					const TSharedPtr<FJsonObject>* List = nullptr;
					if (Value->TryGetObject(List))
					{
						Items.Add({ ProtoBoyTrelloClient::GetString(*List, TEXT("id")), ProtoBoyTrelloClient::GetString(*List, TEXT("name")) });
					}
				}
			}
			// Keep board order (left to right), which is how people think about columns.
			OnDone(TProtoBoyTaskResult<TArray<FProtoBoyTrelloItem>>::Success(MoveTemp(Items)));
		});
}

FString FProtoBoyTrelloClient::GetTemplateListName(EProtoBoyTaskStatus Status)
{
	switch (Status)
	{
	case EProtoBoyTaskStatus::ToDo: return TEXT("To Do");
	case EProtoBoyTaskStatus::InProgress: return TEXT("In Progress");
	case EProtoBoyTaskStatus::Done: return TEXT("Done");
	case EProtoBoyTaskStatus::Other: break;
	}
	return FString();
}

FString FProtoBoyTrelloClient::GuessListId(const TArray<FProtoBoyTrelloItem>& Lists, EProtoBoyTaskStatus Status)
{
	TArray<const TCHAR*> Names;
	switch (Status)
	{
	case EProtoBoyTaskStatus::ToDo: Names = { TEXT("To Do"), TEXT("ToDo"), TEXT("Todo"), TEXT("Backlog"), TEXT("Next"), TEXT("Up Next") }; break;
	case EProtoBoyTaskStatus::InProgress: Names = { TEXT("In Progress"), TEXT("Doing"), TEXT("WIP"), TEXT("In Development") }; break;
	case EProtoBoyTaskStatus::Done: Names = { TEXT("Done"), TEXT("Complete"), TEXT("Completed"), TEXT("Finished") }; break;
	case EProtoBoyTaskStatus::Other: break;
	}

	for (const TCHAR* Name : Names)
	{
		for (const FProtoBoyTrelloItem& List : Lists)
		{
			if (List.Name.TrimStartAndEnd().Equals(Name, ESearchCase::IgnoreCase))
			{
				return List.Id;
			}
		}
	}
	return FString();
}

void FProtoBoyTrelloClient::ApplyBoardTemplate(const FString& ExistingBoardId, const FString& ExistingBoardName, const FString& NewBoardName, FSetupCallback OnDone)
{
	using FResult = TProtoBoyTaskResult<FProtoBoyTrelloBoardSetup>;

	// Shared by the chain of async steps below.
	struct FState
	{
		FProtoBoyTrelloBoardSetup Setup;
		/** Statuses whose list still has to be created, in board order. */
		TArray<EProtoBoyTaskStatus> Missing;
		FSetupCallback OnDone;
		TFunction<void()> CreateNext;
	};
	const TSharedRef<FState> State = MakeShared<FState>();
	State->OnDone = MoveTemp(OnDone);

	auto ListField = [](FProtoBoyTrelloBoardSetup& Setup, EProtoBoyTaskStatus Status) -> FString&
	{
		return Status == EProtoBoyTaskStatus::ToDo ? Setup.ToDoListId
			: Status == EProtoBoyTaskStatus::InProgress ? Setup.InProgressListId
			: Setup.DoneListId;
	};

	// Step 3: create missing lists one at a time, so they land left-to-right in order.
	// Holds State weakly (State owns this function); in-flight requests keep State alive.
	const TWeakPtr<FState> WeakState = State;
	State->CreateNext = [this, WeakState, ListField]()
	{
		const TSharedPtr<FState> State = WeakState.Pin();
		if (!State.IsValid())
		{
			return;
		}
		if (State->Missing.IsEmpty())
		{
			FSetupCallback Callback = MoveTemp(State->OnDone);
			Callback(FResult::Success(State->Setup));
			return;
		}

		const EProtoBoyTaskStatus Status = State->Missing[0];
		State->Missing.RemoveAt(0);

		TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
		Body->SetStringField(TEXT("name"), GetTemplateListName(Status));
		Body->SetStringField(TEXT("idBoard"), State->Setup.BoardId);
		Body->SetStringField(TEXT("pos"), TEXT("bottom"));

		Call(TEXT("POST"), TEXT("lists"), Body, [State, Status, ListField](bool bOk, const FText& Error, const TSharedPtr<FJsonValue>& Json)
		{
			const TSharedPtr<FJsonObject>* List = nullptr;
			if (!bOk || !Json->TryGetObject(List))
			{
				FSetupCallback Callback = MoveTemp(State->OnDone);
				Callback(FResult::Failure(bOk ? LOCTEXT("BadList", "Trello sent an unexpected list.") : Error));
				return;
			}
			ListField(State->Setup, Status) = ProtoBoyTrelloClient::GetString(*List, TEXT("id"));
			State->Setup.CreatedLists.Add(GetTemplateListName(Status));
			State->CreateNext();
		});
	};

	// Step 2: map what the board already has; queue the rest.
	auto MapLists = [State, ListField](const TArray<FProtoBoyTrelloItem>& Lists)
	{
		const UProtoBoySettings* Settings = GetDefault<UProtoBoySettings>();
		const bool bSameBoard = Settings->TrelloBoardId == State->Setup.BoardId;
		auto Exists = [&Lists](const FString& Id) { return !Id.IsEmpty() && Lists.ContainsByPredicate([&Id](const FProtoBoyTrelloItem& L) { return L.Id == Id; }); };

		for (const EProtoBoyTaskStatus Status : { EProtoBoyTaskStatus::ToDo, EProtoBoyTaskStatus::InProgress, EProtoBoyTaskStatus::Done })
		{
			// Keep the team's current mapping if it still points at a list on this board.
			const FString Current = !bSameBoard ? FString()
				: Status == EProtoBoyTaskStatus::ToDo ? Settings->TrelloToDoListId
				: Status == EProtoBoyTaskStatus::InProgress ? Settings->TrelloInProgressListId
				: Settings->TrelloDoneListId;

			FString& Field = ListField(State->Setup, Status);
			Field = Exists(Current) ? Current : GuessListId(Lists, Status);
			if (Field.IsEmpty())
			{
				State->Missing.Add(Status);
			}
		}
		State->CreateNext();
	};

	// Step 1: the board.
	if (!ExistingBoardId.IsEmpty())
	{
		if (!IsSafeId(ExistingBoardId))
		{
			State->OnDone(FResult::Failure(LOCTEXT("BadBoardId", "Invalid board id.")));
			return;
		}
		State->Setup.BoardId = ExistingBoardId;
		State->Setup.BoardName = ExistingBoardName;
		FetchLists(ExistingBoardId, [State, MapLists](const TProtoBoyTaskResult<TArray<FProtoBoyTrelloItem>>& Lists)
		{
			if (!Lists.bOk)
			{
				FSetupCallback Callback = MoveTemp(State->OnDone);
				Callback(FResult::Failure(Lists.Error));
				return;
			}
			MapLists(Lists.Value);
		});
		return;
	}

	const FString Name = NewBoardName.TrimStartAndEnd();
	if (Name.IsEmpty())
	{
		State->OnDone(FResult::Failure(LOCTEXT("NoBoardName", "Enter a name for the new board.")));
		return;
	}

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("name"), Name.Left(512));
	Body->SetBoolField(TEXT("defaultLists"), false);
	Body->SetStringField(TEXT("desc"), FString::Printf(TEXT(
		"Task board for %s, set up by ProtoBoy.\n\n"
		"- Lists: To Do -> In Progress -> Done. AI agents move cards through them via ProtoBoy.\n"
		"- Link each card to its design doc with a line like: Design doc: Design/Combat.md\n"
		"- The project's docs start at the Game doc in the docs folder."), FApp::GetProjectName()));

	Call(TEXT("POST"), TEXT("boards"), Body, [State, MapLists](bool bOk, const FText& Error, const TSharedPtr<FJsonValue>& Json)
	{
		const TSharedPtr<FJsonObject>* Board = nullptr;
		if (!bOk || !Json->TryGetObject(Board))
		{
			FSetupCallback Callback = MoveTemp(State->OnDone);
			Callback(FResult::Failure(bOk ? LOCTEXT("BadNewBoard", "Trello sent an unexpected board.") : Error));
			return;
		}
		State->Setup.BoardId = ProtoBoyTrelloClient::GetString(*Board, TEXT("id"));
		State->Setup.BoardName = ProtoBoyTrelloClient::GetString(*Board, TEXT("name"));
		State->Setup.bCreatedBoard = true;
		// Created without default lists, so everything is missing.
		MapLists({});
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

// --- Task categories ----------------------------------------------------------------------------------------------

FString FProtoBoyTrelloClient::GetCategoryListName(const FString& Category)
{
	return FString::Printf(TEXT("To Do - %s"), *Category.TrimStartAndEnd());
}

bool FProtoBoyTrelloClient::ParseCategoryListName(const FString& ListName, FString& OutCategory)
{
	FString Rest = ListName.TrimStartAndEnd();
	if (!Rest.StartsWith(TEXT("To Do"), ESearchCase::IgnoreCase))
	{
		return false;
	}
	Rest = Rest.RightChop(5).TrimStart();
	// "-" or an en dash (some apps turn one into the other).
	if (!Rest.StartsWith(TEXT("-")) && !Rest.StartsWith(TEXT("–")))
	{
		return false;
	}
	OutCategory = Rest.RightChop(1).TrimStartAndEnd();
	return !OutCategory.IsEmpty();
}

void FProtoBoyTrelloClient::PlanCategorySync(const TArray<FString>& Categories, FCategorySyncCallback OnDone)
{
	using FResult = TProtoBoyTaskResult<FProtoBoyTrelloCategorySync>;
	FText WhyNot;
	if (!IsReady(WhyNot))
	{
		OnDone(FResult::Failure(WhyNot));
		return;
	}
	const UProtoBoySettings* Settings = GetDefault<UProtoBoySettings>();
	if (!IsSafeId(Settings->TrelloBoardId))
	{
		OnDone(FResult::Failure(LOCTEXT("SyncBadBoard", "Invalid board id.")));
		return;
	}

	// Categories the board should have a list for: Uncategorized first, then each category once, in map order.
	TArray<FString> Wanted = { ProtoBoyTasks::UncategorizedName };
	for (const FString& Category : Categories)
	{
		const FString Name = Category.TrimStartAndEnd();
		if (!Name.IsEmpty() && !Wanted.ContainsByPredicate([&Name](const FString& W) { return W.Equals(Name, ESearchCase::IgnoreCase); }))
		{
			Wanted.Add(Name);
		}
	}

	const FString Path = FString::Printf(TEXT("boards/%s/lists?filter=open&fields=name,pos&cards=open&card_fields=id"), *Settings->TrelloBoardId);
	Call(TEXT("GET"), Path, nullptr, [Wanted, OnDone = MoveTemp(OnDone)](bool bOk, const FText& Error, const TSharedPtr<FJsonValue>& Json)
	{
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (!bOk || !Json->TryGetArray(Array))
		{
			OnDone(FResult::Failure(bOk ? LOCTEXT("SyncBadLists", "Trello sent an unexpected list of lists.") : Error));
			return;
		}

		struct FList
		{
			FString Id;
			FString Name;
			double Pos = 0.0;
			int32 NumCards = 0;
		};
		TArray<FList> Lists;
		for (const TSharedPtr<FJsonValue>& Value : *Array)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (!Value->TryGetObject(Object))
			{
				continue;
			}
			FList& List = Lists.AddDefaulted_GetRef();
			List.Id = ProtoBoyTrelloClient::GetString(*Object, TEXT("id"));
			List.Name = ProtoBoyTrelloClient::GetString(*Object, TEXT("name"));
			(*Object)->TryGetNumberField(TEXT("pos"), List.Pos);
			const TArray<TSharedPtr<FJsonValue>>* Cards = nullptr;
			if ((*Object)->TryGetArrayField(TEXT("cards"), Cards))
			{
				List.NumCards = Cards->Num();
			}
		}
		Lists.Sort([](const FList& A, const FList& B) { return A.Pos < B.Pos; });

		const UProtoBoySettings* Settings = GetDefault<UProtoBoySettings>();
		FProtoBoyTrelloCategorySync Plan;
		TArray<FString> Found;
		int32 LastToDoIndex = INDEX_NONE;
		for (int32 Index = 0; Index < Lists.Num(); ++Index)
		{
			const FList& List = Lists[Index];
			FString Category;
			if (!ParseCategoryListName(List.Name, Category))
			{
				// The team's plain "To Do" list counts as where the To Do lists are, for placing new ones.
				if (List.Id == Settings->TrelloToDoListId)
				{
					LastToDoIndex = Index;
				}
				continue;
			}
			// Never archive the lists ProtoBoy maps In Progress and Done to, whatever they're called.
			if (List.Id == Settings->TrelloInProgressListId || List.Id == Settings->TrelloDoneListId)
			{
				continue;
			}
			if (Wanted.ContainsByPredicate([&Category](const FString& W) { return W.Equals(Category, ESearchCase::IgnoreCase); }))
			{
				Found.Add(Category);
				LastToDoIndex = Index;
				if (ProtoBoyTasks::IsReservedCategoryName(Category) && Plan.UncategorizedListId.IsEmpty())
				{
					Plan.UncategorizedListId = List.Id;
				}
				continue;
			}
			Plan.ListsToRemove.Add({ List.Id, List.Name, List.NumCards });
			Plan.bRemapsToDoList |= List.Id == Settings->TrelloToDoListId;
		}

		for (const FString& Category : Wanted)
		{
			if (!Found.ContainsByPredicate([&Category](const FString& F) { return F.Equals(Category, ESearchCase::IgnoreCase); }))
			{
				Plan.ListsToCreate.Add(GetCategoryListName(Category));
			}
		}

		// New lists go right after the last To Do list that stays (else at the left edge of the board).
		if (LastToDoIndex != INDEX_NONE)
		{
			Plan.InsertAfterPos = Lists[LastToDoIndex].Pos;
			if (Lists.IsValidIndex(LastToDoIndex + 1))
			{
				Plan.InsertBeforePos = Lists[LastToDoIndex + 1].Pos;
			}
		}
		else if (!Lists.IsEmpty())
		{
			Plan.InsertBeforePos = Lists[0].Pos;
		}
		OnDone(FResult::Success(MoveTemp(Plan)));
	});
}

void FProtoBoyTrelloClient::ApplyCategorySync(const FProtoBoyTrelloCategorySync& Plan, FDoneCallback OnDone)
{
	using FResult = TProtoBoyTaskResult<bool>;
	FText WhyNot;
	if (!IsReady(WhyNot))
	{
		OnDone(FResult::Failure(WhyNot));
		return;
	}
	const FString BoardId = GetDefault<UProtoBoySettings>()->TrelloBoardId;
	if (!IsSafeId(BoardId))
	{
		OnDone(FResult::Failure(LOCTEXT("SyncBadBoard", "Invalid board id.")));
		return;
	}
	for (const FProtoBoyTrelloCategorySync::FExtraList& List : Plan.ListsToRemove)
	{
		if (!IsSafeId(List.Id))
		{
			OnDone(FResult::Failure(LOCTEXT("SyncBadListId", "Invalid list id.")));
			return;
		}
	}

	// One request at a time, in order, sharing this state (same pattern as ApplyBoardTemplate).
	using FStepDone = TFunction<void(bool, const FText&)>;
	using FStep = TFunction<void(FStepDone)>;
	struct FState
	{
		TArray<FStep> Steps;
		int32 Next = 0;
		FString UncategorizedListId;
		FDoneCallback OnDone;
		TFunction<void()> RunNext;
	};
	const TSharedRef<FState> State = MakeShared<FState>();
	State->OnDone = MoveTemp(OnDone);
	State->UncategorizedListId = Plan.UncategorizedListId;
	const TWeakPtr<FState> WeakState = State;

	// Create the missing lists, spaced evenly between the insert positions so they keep their order.
	const int32 NumNew = Plan.ListsToCreate.Num();
	for (int32 Index = 0; Index < NumNew; ++Index)
	{
		TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
		const FString Name = Plan.ListsToCreate[Index];
		Body->SetStringField(TEXT("name"), Name);
		Body->SetStringField(TEXT("idBoard"), BoardId);
		const double Fraction = double(Index + 1) / double(NumNew + 1);
		if (Plan.InsertAfterPos.IsSet() && Plan.InsertBeforePos.IsSet())
		{
			Body->SetNumberField(TEXT("pos"), Plan.InsertAfterPos.GetValue() + (Plan.InsertBeforePos.GetValue() - Plan.InsertAfterPos.GetValue()) * Fraction);
		}
		else if (Plan.InsertAfterPos.IsSet())
		{
			Body->SetNumberField(TEXT("pos"), Plan.InsertAfterPos.GetValue() + 16384.0 * (Index + 1));
		}
		else if (Plan.InsertBeforePos.IsSet())
		{
			Body->SetNumberField(TEXT("pos"), Plan.InsertBeforePos.GetValue() * Fraction);
		}
		else
		{
			Body->SetStringField(TEXT("pos"), TEXT("bottom"));
		}

		State->Steps.Add([this, Body, Name, WeakState](FStepDone StepDone)
		{
			Call(TEXT("POST"), TEXT("lists"), Body, [Name, WeakState, StepDone = MoveTemp(StepDone)](bool bOk, const FText& Error, const TSharedPtr<FJsonValue>& Json)
			{
				const TSharedPtr<FJsonObject>* List = nullptr;
				if (!bOk || !Json->TryGetObject(List))
				{
					StepDone(false, bOk ? LOCTEXT("SyncBadNewList", "Trello sent an unexpected list.") : Error);
					return;
				}
				FString Category;
				const TSharedPtr<FState> Pinned = WeakState.Pin();
				if (Pinned.IsValid() && ParseCategoryListName(Name, Category) && ProtoBoyTasks::IsReservedCategoryName(Category))
				{
					Pinned->UncategorizedListId = ProtoBoyTrelloClient::GetString(*List, TEXT("id"));
				}
				StepDone(true, FText::GetEmpty());
			});
		});
	}

	// Move each extra list's cards to Uncategorized, then archive it.
	for (const FProtoBoyTrelloCategorySync::FExtraList& List : Plan.ListsToRemove)
	{
		const FString ListId = List.Id;
		if (List.NumCards > 0)
		{
			State->Steps.Add([this, ListId, BoardId, WeakState](FStepDone StepDone)
			{
				const TSharedPtr<FState> Pinned = WeakState.Pin();
				if (!Pinned.IsValid() || !IsSafeId(Pinned->UncategorizedListId))
				{
					StepDone(false, LOCTEXT("SyncNoUncategorized", "The To Do - Uncategorized list is missing, so the cards had nowhere to go."));
					return;
				}
				Call(TEXT("POST"), FString::Printf(TEXT("lists/%s/moveAllCards?idBoard=%s&idList=%s"), *ListId, *BoardId, *Pinned->UncategorizedListId), nullptr,
					[StepDone = MoveTemp(StepDone)](bool bOk, const FText& Error, const TSharedPtr<FJsonValue>&) { StepDone(bOk, Error); });
			});
		}
		State->Steps.Add([this, ListId](FStepDone StepDone)
		{
			Call(TEXT("PUT"), FString::Printf(TEXT("lists/%s/closed?value=true"), *ListId), nullptr,
				[StepDone = MoveTemp(StepDone)](bool bOk, const FText& Error, const TSharedPtr<FJsonValue>&) { StepDone(bOk, Error); });
		});
	}

	// Holds State weakly (State owns this function); each step's callback holds it strongly while a request is in flight.
	const bool bRemapsToDoList = Plan.bRemapsToDoList;
	State->RunNext = [WeakState, bRemapsToDoList]()
	{
		const TSharedPtr<FState> Pinned = WeakState.Pin();
		if (!Pinned.IsValid())
		{
			return;
		}
		if (!Pinned->Steps.IsValidIndex(Pinned->Next))
		{
			if (bRemapsToDoList && !Pinned->UncategorizedListId.IsEmpty())
			{
				UProtoBoySettings* Settings = GetMutableDefault<UProtoBoySettings>();
				Settings->TrelloToDoListId = Pinned->UncategorizedListId;
				Settings->TryUpdateDefaultConfigFile();
			}
			FDoneCallback Callback = MoveTemp(Pinned->OnDone);
			Callback(FResult::Success(true));
			return;
		}
		const TSharedRef<FState> Strong = Pinned.ToSharedRef();
		Pinned->Steps[Pinned->Next++]([Strong](bool bOk, const FText& Error)
		{
			if (!bOk)
			{
				FDoneCallback Callback = MoveTemp(Strong->OnDone);
				Callback(FResult::Failure(Error));
				return;
			}
			Strong->RunNext();
		});
	};
	State->RunNext();
}

#undef LOCTEXT_NAMESPACE
