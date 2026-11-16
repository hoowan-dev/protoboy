// The board picker and ProtoBoy's board template (Connect Task Board > Trello). Part of FProtoBoyTrelloClient.
#include "Tasks/Trello/PBTrelloClient.h"

#include "Misc/App.h"
#include "PBSettings.h"
#include "Tasks/Trello/PBTrelloJson.h"

#define LOCTEXT_NAMESPACE "ProtoBoyTrelloBoardSetup"

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

#undef LOCTEXT_NAMESPACE
