#pragma once

#include "CoreMinimal.h"

/** Where a task stands, mapped from the provider's own columns/states (see the Trello list mapping). */
enum class EProtoBoyTaskStatus : uint8
{
	ToDo,
	InProgress,
	Done,
	/** In a column ProtoBoy doesn't map. */
	Other,
};

struct FProtoBoyTaskComment
{
	FString Author;
	FString Text;
	FString DateUtc;
};

struct FProtoBoyTask
{
	FString Id;
	FString Title;
	FString Description;
	FString Url;
	EProtoBoyTaskStatus Status = EProtoBoyTaskStatus::Other;
	/** The provider's own column/state name, e.g. the Trello list name. */
	FString StatusName;
	TArray<FString> Labels;
	FString DueUtc;
	FString LastActivityUtc;
	/** Design doc path linked from the description ("Design doc: Design/Combat.md"), if any. */
	FString DesignDoc;
	/** Only filled by GetTask. */
	TArray<FProtoBoyTaskComment> Comments;
};

/** A task to create in a task category's To Do list (IProtoBoyTaskProvider::CreateTasks). */
struct FProtoBoyNewTask
{
	/** A task category's title; empty or unknown = Uncategorized. */
	FString Category;
	FString Title;
	FString Description;
};

/** Result of an async task call. Error is set when bOk is false. */
template <typename T>
struct TProtoBoyTaskResult
{
	bool bOk = false;
	FText Error;
	T Value;

	static TProtoBoyTaskResult Success(T InValue) { TProtoBoyTaskResult R; R.bOk = true; R.Value = MoveTemp(InValue); return R; }
	static TProtoBoyTaskResult Failure(const FText& InError) { TProtoBoyTaskResult R; R.Error = InError; return R; }
};

/**
 * A task board (Trello; later Jira, Basecamp, ...). Calls are async; callbacks run on the game thread.
 * Implementations limit every call to the team's configured board.
 */
class PROTOBOY_API IProtoBoyTaskProvider
{
public:
	using FTasksCallback = TFunction<void(const TProtoBoyTaskResult<TArray<FProtoBoyTask>>&)>;
	using FTaskCallback = TFunction<void(const TProtoBoyTaskResult<FProtoBoyTask>&)>;
	using FDoneCallback = TFunction<void(const TProtoBoyTaskResult<bool>&)>;
	/** One created task (or why it failed), by its index in the list passed to CreateTasks. Return false to stop there. */
	using FEachTaskCallback = TFunction<bool(int32 Index, const TProtoBoyTaskResult<FProtoBoyTask>&)>;

	virtual ~IProtoBoyTaskProvider() = default;

	/** E.g. "Trello (board \"Game\")". */
	virtual FText GetDisplayName() const = 0;

	/** False with a reason if the board can't be used (not connected, no board chosen, lists not mapped). */
	virtual bool IsReady(FText& OutWhyNot) const = 0;

	/** Open tasks on the board. Filter limits to one status; unset returns all. */
	virtual void ListTasks(TOptional<EProtoBoyTaskStatus> Filter, FTasksCallback OnDone) = 0;
	virtual void GetTask(const FString& Id, FTaskCallback OnDone) = 0;
	virtual void CreateTask(const FString& Title, const FString& Description, EProtoBoyTaskStatus Status, FTaskCallback OnDone) = 0;
	virtual void SetTaskStatus(const FString& Id, EProtoBoyTaskStatus Status, FTaskCallback OnDone) = 0;
	virtual void AddComment(const FString& Id, const FString& Text, FDoneCallback OnDone) = 0;
	/**
	 * Create tasks in their categories' To Do lists, one at a time, tagged with the category. A category with no list of
	 * its own goes to Uncategorized (else the plain To Do list). OnEach runs per task, OnDone once at the end; one failed
	 * task doesn't stop the rest (OnDone fails only if none could be created).
	 */
	virtual void CreateTasks(const TArray<FProtoBoyNewTask>& Tasks, FEachTaskCallback OnEach, FDoneCallback OnDone) = 0;
};

namespace ProtoBoyTasks
{
	/**
	 * The catch-all task category: tasks that belong to no doc. The board has a "To Do - Uncategorized" list for it, so
	 * no doc may be titled this (FProtoBoyDocMap refuses it).
	 */
	PROTOBOY_API extern const TCHAR* UncategorizedName;
	/** True for "Uncategorized" (any case, surrounding spaces ignored). */
	PROTOBOY_API bool IsReservedCategoryName(const FString& Name);

	/** "todo", "in_progress", "done", "other" (the names agents use). */
	PROTOBOY_API FString StatusToString(EProtoBoyTaskStatus Status);
	PROTOBOY_API TOptional<EProtoBoyTaskStatus> StatusFromString(const FString& Value);

	/** Line ProtoBoy writes into task descriptions to link a design doc. */
	PROTOBOY_API FString MakeDesignDocLine(const FString& DocPath);
	/** The design doc path from a "Design doc: ..." line, or empty. */
	PROTOBOY_API FString FindDesignDoc(const FString& Description);
}
