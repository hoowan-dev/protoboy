#include "Tasks/PBTaskBoard.h"

#include "Docs/PBDocMap.h"
#include "Tasks/PBTasksConnection.h"
#include "Tasks/Trello/PBTrelloClient.h"
#include "Tasks/Trello/PBTrelloConnector.h"

FProtoBoyTaskBoard::FProtoBoyTaskBoard(FProtoBoyTrelloClient& InClient, FProtoBoyTrelloConnector& InConnector, FProtoBoyDocMap& InMap)
	: Client(InClient)
	, Connector(InConnector)
	, Map(InMap)
{
	DocMapChangedHandle = Map.OnChanged.AddRaw(this, &FProtoBoyTaskBoard::OnDocMapChanged);
}

FProtoBoyTaskBoard::~FProtoBoyTaskBoard()
{
	Map.OnChanged.Remove(DocMapChangedHandle);
}

TArray<FProtoBoyTaskCategory> FProtoBoyTaskBoard::GetCategories() const
{
	TArray<FProtoBoyTaskCategory> Categories;
	for (const FProtoBoyDocNode* Doc : Map.GetTaskCategories())
	{
		Categories.Add({ Doc->Id, Doc->Title });
	}
	return Categories;
}

FText FProtoBoyTaskBoard::GetServiceName() const
{
	return ProtoBoyTasks::GetProviderName(Connector.GetConnection().Provider);
}

void FProtoBoyTaskBoard::CheckSync(FOnOutOfSync OnOutOfSync)
{
	const TArray<FProtoBoyTaskCategory> Categories = GetCategories();
	SyncState = ESyncState::Checking;
	CheckedCategories = Categories;
	const int32 Serial = ++CheckSerial;
	OnChanged.Broadcast();

	const TWeakPtr<int32> WeakAlive = Alive;
	Client.PlanCategorySync(Categories, [this, WeakAlive, Serial, Categories, OnOutOfSync = MoveTemp(OnOutOfSync)](const TProtoBoyTaskResult<FProtoBoyTrelloCategorySync>& Result)
	{
		if (!WeakAlive.IsValid() || Serial != CheckSerial)
		{
			return;
		}
		if (!Result.bOk)
		{
			SyncState = ESyncState::Failed;
			SyncError = Result.Error;
			OnChanged.Broadcast();
			return;
		}

		// Every check refreshes the tasks, in sync or not.
		const FProtoBoyTrelloCategorySync& Plan = Result.Value;
		Lists = Plan.CategoryLists;
		InProgress = Plan.InProgressTasks;
		Done = Plan.DoneTasks;
		bLoaded = true;

		if (GetCategories() != Categories)
		{
			// The categories changed while the board answered.
			SyncState = ESyncState::Unknown;
		}
		else if (Plan.IsInSync())
		{
			SyncState = ESyncState::InSync;
			// Lists and labels matched by name are remembered by id from now on, so renames keep them.
			FProtoBoyTrelloClient::RememberCategoryIds(Plan.ListIds, Plan.LabelIds);
			Connector.SetLastTaskSync(FDateTime::UtcNow());
		}
		else
		{
			SyncState = ESyncState::OutOfSync;
		}
		OnChanged.Broadcast();

		if (SyncState == ESyncState::OutOfSync && OnOutOfSync)
		{
			OnOutOfSync(Plan);
		}
	});
}

void FProtoBoyTaskBoard::ApplySync(const FProtoBoyTrelloCategorySync& Plan, TFunction<void(bool bOk, const FText& Error)> OnDone)
{
	const TWeakPtr<int32> WeakAlive = Alive;
	Client.ApplyCategorySync(Plan, [this, WeakAlive, OnDone = MoveTemp(OnDone)](const TProtoBoyTaskResult<bool>& Result)
	{
		if (OnDone)
		{
			OnDone(Result.bOk, Result.Error);
		}
		// Show what the board looks like now, whether or not every step went through.
		if (WeakAlive.IsValid())
		{
			CheckSync();
		}
	});
}

TOptional<FDateTime> FProtoBoyTaskBoard::GetLastSync() const
{
	FDateTime When;
	return FDateTime::ParseIso8601(*Connector.GetConnection().LastTaskSyncUtc, When) ? TOptional<FDateTime>(When) : TOptional<FDateTime>();
}

const FProtoBoyTrelloCategorySync::FCategoryList* FProtoBoyTaskBoard::FindList(const FString& DocId) const
{
	return Lists.FindByPredicate([&DocId](const FProtoBoyTrelloCategorySync::FCategoryList& List) { return List.DocId == DocId; });
}

void FProtoBoyTaskBoard::OnDocMapChanged()
{
	if (SyncState != ESyncState::Checking && SyncState != ESyncState::Unknown && GetCategories() != CheckedCategories)
	{
		SyncState = ESyncState::Unknown;
		OnChanged.Broadcast();
	}
}
