#pragma once

#include "CoreMinimal.h"
#include "Tasks/PBTaskProvider.h"
#include "Tasks/Trello/PBTrelloCategorySync.h"

class FProtoBoyDocMap;
class FProtoBoyTrelloClient;
class FProtoBoyTrelloConnector;

/**
 * The task board as ProtoBoy last saw it: whether it matches the doc map's task categories, and the tasks in each
 * category's To Do list and in In Progress and Done. Owned by UProtoBoySubsystem, so it outlives the dashboard; the
 * Tasks page, the out-of-sync window and task generation all read it here. Filled by CheckSync (the Sync button, and
 * after any change ProtoBoy makes to the board); nothing is read from the board until then.
 *
 * Trello is the only board today, so this talks to FProtoBoyTrelloClient for the category sync.
 */
class PROTOBOY_API FProtoBoyTaskBoard
{
public:
	enum class ESyncState : uint8
	{
		/** Not checked yet, or the categories changed since. */
		Unknown,
		Checking,
		InSync,
		OutOfSync,
		/** The board couldn't be read (GetSyncError says why). */
		Failed,
	};

	using FOnOutOfSync = TFunction<void(const FProtoBoyTrelloCategorySync& Plan)>;

	FProtoBoyTaskBoard(FProtoBoyTrelloClient& InClient, FProtoBoyTrelloConnector& InConnector, FProtoBoyDocMap& InMap);
	~FProtoBoyTaskBoard();

	/** The doc map's task categories, in map order. */
	TArray<FProtoBoyTaskCategory> GetCategories() const;
	/** The board service's name, e.g. "Trello". */
	FText GetServiceName() const;

	/**
	 * Compare the board with the task categories and load its tasks. When they don't match, OnOutOfSync gets the plan
	 * that would fix it (FProtoBoyTrelloCategorySync). Fires OnChanged when the answer arrives.
	 */
	void CheckSync(FOnOutOfSync OnOutOfSync = nullptr);
	/** Make the board match Plan, then check again. OnDone gets the error if a step failed (what was done is kept). */
	void ApplySync(const FProtoBoyTrelloCategorySync& Plan, TFunction<void(bool bOk, const FText& Error)> OnDone);

	ESyncState GetSyncState() const { return SyncState; }
	const FText& GetSyncError() const { return SyncError; }
	/** When the board last matched the categories (UTC), from the user's connection; unset if never. */
	TOptional<FDateTime> GetLastSync() const;

	/** True once a check has read the board's tasks. */
	bool IsLoaded() const { return bLoaded; }
	/** A category's To Do list (empty DocId = Uncategorized), from the last check; null if the board has none yet. */
	const FProtoBoyTrelloCategorySync::FCategoryList* FindList(const FString& DocId) const;
	const TArray<FProtoBoyTask>& GetInProgress() const { return InProgress; }
	/** Most recently active first. */
	const TArray<FProtoBoyTask>& GetDone() const { return Done; }

	/** Fired when the sync state or the loaded tasks change. */
	FSimpleMulticastDelegate OnChanged;

private:
	/** The categories changed: a sync answer about the old ones no longer says anything. */
	void OnDocMapChanged();

	FProtoBoyTrelloClient& Client;
	FProtoBoyTrelloConnector& Connector;
	FProtoBoyDocMap& Map;
	FDelegateHandle DocMapChangedHandle;

	ESyncState SyncState = ESyncState::Unknown;
	FText SyncError;
	/** The categories SyncState is about. */
	TArray<FProtoBoyTaskCategory> CheckedCategories;
	/** Bumped by every check, so an older answer arriving late is ignored. */
	int32 CheckSerial = 0;

	bool bLoaded = false;
	TArray<FProtoBoyTrelloCategorySync::FCategoryList> Lists;
	TArray<FProtoBoyTask> InProgress;
	TArray<FProtoBoyTask> Done;

	/** Callbacks hold this weakly, so an answer arriving after shutdown is dropped. */
	TSharedRef<int32> Alive = MakeShared<int32>(0);
};
