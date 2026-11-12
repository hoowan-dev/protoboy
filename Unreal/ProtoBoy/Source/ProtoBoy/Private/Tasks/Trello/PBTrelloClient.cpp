#include "Tasks/Trello/PBTrelloClient.h"

#include "Dom/JsonObject.h"
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

#undef LOCTEXT_NAMESPACE
