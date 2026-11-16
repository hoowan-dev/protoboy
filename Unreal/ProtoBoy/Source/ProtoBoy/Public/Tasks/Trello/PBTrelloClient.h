#pragma once

#include "Tasks/PBTaskProvider.h"
#include "Tasks/Trello/PBTrelloCategorySync.h"

class FProtoBoyTrelloConnector;
namespace ProtoBoyTrelloCategorySync { struct FList; }
class FJsonObject;

/** A Trello board or list, for the board picker. */
struct FProtoBoyTrelloItem
{
	FString Id;
	FString Name;
};

/** Result of applying ProtoBoy's board template: the board and the lists to save as the team's mapping. */
struct FProtoBoyTrelloBoardSetup
{
	FString BoardId;
	FString BoardName;
	FString ToDoListId;
	FString InProgressListId;
	FString DoneListId;
	bool bCreatedBoard = false;
	/** Names of the lists the template had to create. */
	TArray<FString> CreatedLists;
};

/**
 * Trello REST client and task provider. Uses the user's token from FProtoBoyTrelloConnector and the team's board and
 * list mapping from UProtoBoySettings (TrelloBoardId, TrelloToDoListId, ...). Every task call is limited to that board.
 * Implemented in three files: PBTrelloClient.cpp (REST calls and the task provider), PBTrelloBoardSetup.cpp (board
 * picker and template) and PBTrelloCategorySync.cpp (task category lists and labels).
 */
class PROTOBOY_API FProtoBoyTrelloClient : public IProtoBoyTaskProvider
{
public:
	using FItemsCallback = TFunction<void(const TProtoBoyTaskResult<TArray<FProtoBoyTrelloItem>>&)>;
	using FSetupCallback = TFunction<void(const TProtoBoyTaskResult<FProtoBoyTrelloBoardSetup>&)>;

	explicit FProtoBoyTrelloClient(const FProtoBoyTrelloConnector& InConnector);

	// Board picker
	void FetchBoards(FItemsCallback OnDone);
	void FetchLists(const FString& BoardId, FItemsCallback OnDone);

	/**
	 * ProtoBoy's board template: "To Do", "In Progress" and "Done" lists.
	 * With ExistingBoardId empty, creates a new board named NewBoardName (with a description of ProtoBoy's conventions).
	 * Otherwise keeps the board's lists that are already mapped or recognizable by name and creates only the missing ones.
	 * Doesn't save settings; the caller stores the returned mapping.
	 */
	void ApplyBoardTemplate(const FString& ExistingBoardId, const FString& ExistingBoardName, const FString& NewBoardName, FSetupCallback OnDone);

	// Task categories
	using FCategorySyncCallback = TFunction<void(const TProtoBoyTaskResult<FProtoBoyTrelloCategorySync>&)>;

	/** "To Do - <Category>". */
	static FString GetCategoryListName(const FString& Category);
	/** The category in a "To Do - <Category>" list name (any case, "-" or an en dash). False for other lists. */
	static bool ParseCategoryListName(const FString& ListName, FString& OutCategory);

	/**
	 * Compare the configured board's lists and labels with Categories (in map order), and read the open cards in each
	 * category's list and in the In Progress and Done lists. Changes nothing on the board.
	 */
	void PlanCategorySync(const TArray<FProtoBoyTaskCategory>& Categories, FCategorySyncCallback OnDone);
	/**
	 * Carry out a plan: rename lists and labels after their docs, create the missing labels and lists, for each extra
	 * list move its cards to "To Do - Uncategorized" and archive it, then add and remove the cards' category labels.
	 * Stops at the first error. Remembers each category's list and label (RememberCategoryIds). If the mapped To Do list
	 * was archived, TrelloToDoListId moves to Uncategorized.
	 */
	void ApplyCategorySync(const FProtoBoyTrelloCategorySync& Plan, FDoneCallback OnDone);
	/**
	 * Save which list and label belong to each category (doc id -> id) in the team settings, if they changed. Only the
	 * given categories are kept, so former categories are forgotten.
	 */
	static void RememberCategoryIds(const TMap<FString, FString>& ListIds, const TMap<FString, FString>& LabelIds);

	/** The list for a status: by name ("To Do", "Doing", "Done", ...). Empty if none matches. */
	static FString GuessListId(const TArray<FProtoBoyTrelloItem>& Lists, EProtoBoyTaskStatus Status);
	/** The list name ProtoBoy's template uses for a status. */
	static FString GetTemplateListName(EProtoBoyTaskStatus Status);

	// IProtoBoyTaskProvider
	virtual FText GetDisplayName() const override;
	virtual bool IsReady(FText& OutWhyNot) const override;
	virtual void ListTasks(TOptional<EProtoBoyTaskStatus> Filter, FTasksCallback OnDone) override;
	virtual void GetTask(const FString& Id, FTaskCallback OnDone) override;
	virtual void CreateTask(const FString& Title, const FString& Description, EProtoBoyTaskStatus Status, FTaskCallback OnDone) override;
	virtual void SetTaskStatus(const FString& Id, EProtoBoyTaskStatus Status, FTaskCallback OnDone) override;
	virtual void AddComment(const FString& Id, const FString& Text, FDoneCallback OnDone) override;
	/** Into "To Do - <Category>" with the category's board label (if the board has one yet; Sync adds it otherwise). */
	virtual void CreateTasks(const TArray<FProtoBoyNewTask>& Tasks, FEachTaskCallback OnEach, FDoneCallback OnDone) override;

private:
	using FJsonCallback = TFunction<void(bool bOk, const FText& Error, const TSharedPtr<class FJsonValue>& Json)>;

	/**
	 * Authenticated call to https://api.trello.com/1/<Path>. When Trello says it's rate limited (HTTP 429, nothing was
	 * done), the call is retried after Retry-After or a doubling delay, up to MaxRetries times.
	 */
	void Call(const FString& Verb, const FString& Path, const TSharedPtr<FJsonObject>& Body, FJsonCallback OnDone) const;
	void CallAttempt(const FString& Verb, const FString& Path, const FString& Body, FJsonCallback OnDone, int32 Attempt) const;
	static constexpr int32 MaxRetries = 5;

	/** Fetch a card and make sure it belongs to the configured board before acting on it. */
	void GetCardOnBoard(const FString& Id, const FString& ExtraQuery, FJsonCallback OnDone) const;

	/** The category plan from the board's lists (with their cards) and labels. See PBTrelloCategorySync.cpp. */
	FProtoBoyTrelloCategorySync BuildCategoryPlan(const TArray<FProtoBoyTaskCategory>& Categories, const TArray<ProtoBoyTrelloCategorySync::FList>& Lists,
		const TArray<FProtoBoyTrelloItem>& Labels, const TMap<FString, FString>& ListNames) const;

	FProtoBoyTask ParseCard(const TSharedPtr<FJsonObject>& Card, const TMap<FString, FString>& ListNames) const;
	FString ListIdFor(EProtoBoyTaskStatus Status) const;
	EProtoBoyTaskStatus StatusForList(const FString& ListId) const;

	static bool IsSafeId(const FString& Id);

	const FProtoBoyTrelloConnector& Connector;
};
