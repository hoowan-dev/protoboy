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
