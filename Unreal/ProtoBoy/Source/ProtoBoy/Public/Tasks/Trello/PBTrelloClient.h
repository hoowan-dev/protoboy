#pragma once

#include "Tasks/PBTaskProvider.h"

class FProtoBoyTrelloConnector;
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
 * What it takes to make the board's To Do lists match the doc map's task categories: one "To Do - Uncategorized" list
 * plus one "To Do - <Category>" list per task category. Other lists are left alone.
 */
struct FProtoBoyTrelloCategorySync
{
	/** A "To Do - <Category>" list whose category no longer exists. */
	struct FExtraList
	{
		FString Id;
		FString Name;
		int32 NumCards = 0;
	};

	/** A "To Do - <Category>" list that matches a task category (or Uncategorized), with its open cards in board order. */
	struct FCategoryList
	{
		/** The category as the list names it (e.g. "Combat"). */
		FString Category;
		FString ListId;
		TArray<FProtoBoyTask> Tasks;
	};

	/** A category label to add to a card, or another category's label to take off it. */
	struct FLabelChange
	{
		FString CardId;
		/** The category the label is named after. */
		FString LabelName;
		/** Empty when the label is in LabelsToCreate (resolved when it's created). */
		FString LabelId;
		bool bAdd = true;
		/** The list the card ends up in, for the window ("To Do - Combat"). */
		FString ListName;
	};

	/** The lists that stay, with their tasks: what the Tasks page shows under each category. */
	TArray<FCategoryList> CategoryLists;
	/** Open cards in the team's In Progress list (TrelloInProgressListId), in board order. */
	TArray<FProtoBoyTask> InProgressTasks;
	/** Open cards in the team's Done list (TrelloDoneListId), most recently active first. */
	TArray<FProtoBoyTask> DoneTasks;
	/** Names of the lists to create, in board order ("To Do - Uncategorized" first when it's missing). */
	TArray<FString> ListsToCreate;
	/** Lists whose cards move to "To Do - Uncategorized" before the list is archived (Trello can't delete lists). */
	TArray<FExtraList> ListsToRemove;
	/** The existing "To Do - Uncategorized" list; empty when it has to be created. */
	FString UncategorizedListId;
	/** True when the team's mapped To Do list (TrelloToDoListId) is one of ListsToRemove: it moves to Uncategorized. */
	bool bRemapsToDoList = false;
	/** Categories with no board label of that name yet. Uncategorized gets none. */
	TArray<FString> LabelsToCreate;
	/**
	 * Every card in a "To Do - <Category>" list carries that category's label and no other category's, so a card can be
	 * traced to its category after it moves on (In Progress, Done). Cards in Uncategorized (and those moved there from
	 * ListsToRemove) carry no category label.
	 */
	TArray<FLabelChange> LabelChanges;
	/** New lists go between these positions (board order), after the existing To Do lists. Unset = board edge. */
	TOptional<double> InsertAfterPos;
	TOptional<double> InsertBeforePos;

	bool IsInSync() const { return ListsToCreate.IsEmpty() && ListsToRemove.IsEmpty() && LabelsToCreate.IsEmpty() && LabelChanges.IsEmpty(); }
};

/**
 * Trello REST client and task provider. Uses the user's token from FProtoBoyTrelloConnector and the team's board and
 * list mapping from UProtoBoySettings (TrelloBoardId, TrelloToDoListId, ...). Every task call is limited to that board.
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
	 * Compare the configured board's lists with Categories (task category titles, in map order), and read the open cards
	 * in each matching list (FProtoBoyTrelloCategorySync::CategoryLists). Changes nothing.
	 */
	void PlanCategorySync(const TArray<FString>& Categories, FCategorySyncCallback OnDone);
	/**
	 * Carry out a plan: create the missing labels and lists, for each extra list move its cards to "To Do - Uncategorized"
	 * and archive it, then add and remove the cards' category labels. Stops at the first error. If the mapped To Do list
	 * was archived, TrelloToDoListId moves to Uncategorized.
	 */
	void ApplyCategorySync(const FProtoBoyTrelloCategorySync& Plan, FDoneCallback OnDone);

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

	/** Authenticated call to https://api.trello.com/1/<Path>. */
	void Call(const FString& Verb, const FString& Path, const TSharedPtr<FJsonObject>& Body, FJsonCallback OnDone) const;

	/** Fetch a card and make sure it belongs to the configured board before acting on it. */
	void GetCardOnBoard(const FString& Id, const FString& ExtraQuery, FJsonCallback OnDone) const;

	FProtoBoyTask ParseCard(const TSharedPtr<FJsonObject>& Card, const TMap<FString, FString>& ListNames) const;
	FString ListIdFor(EProtoBoyTaskStatus Status) const;
	EProtoBoyTaskStatus StatusForList(const FString& ListId) const;

	static bool IsSafeId(const FString& Id);

	const FProtoBoyTrelloConnector& Connector;
};
