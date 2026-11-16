#pragma once

#include "CoreMinimal.h"
#include "Agents/PBAgentRun.h"
#include "Tasks/PBTaskProvider.h"

/**
 * Generates task ideas for task categories with one of the user's agents (Tasks > Generate Tasks), through
 * FProtoBoyAgentRun. The prompt carries each picked category's doc and the docs below it on the doc map (the docs may
 * live outside the project folder, e.g. in an Obsidian vault, where the agent can't read), the tasks already in the
 * category's To Do list so it doesn't repeat them, and the user's extra context. The agent reads the project (read
 * only) and answers with tasks as JSON, which become GetIdeas(). Nothing goes to the board until the user picks the
 * ideas to keep and AddTasks adds them to each category's To Do list (IProtoBoyTaskProvider::CreateTasks), linking the
 * category's doc.
 *
 * Cancel stops the agent, or stops adding; tasks already added stay on the board. Tick() is polled by the UI (the
 * window is modal). The prompt and raw output are kept in Saved/ProtoBoy/TaskGeneration/.
 */
class PROTOBOY_API FProtoBoyTaskGenerationRun : public TSharedFromThis<FProtoBoyTaskGenerationRun>
{
public:
	~FProtoBoyTaskGenerationRun();

	/**
	 * Write the prompt and start the agent for the task categories with these doc ids (in map order). The tasks already
	 * in each one's To Do list come from FProtoBoyTaskBoard (the last Sync). False (nothing started) with a reason. Use a
	 * new run for each attempt.
	 */
	bool Start(EProtoBoyMCPClient Client, const TArray<FString>& CategoryIds, const FString& ExtraContext, FText& OutError);
	/** Read the agent's output, and once it exits, read the ideas it returned. Call often while the agent runs. */
	void Tick();
	/** Stop the agent, or stop adding tasks. Tasks already on the board stay. */
	void Cancel();

	/** The agent is still working. */
	bool IsRunning() const { return Agent.IsRunning(); }
	/** The agent is done (or was cancelled). */
	bool IsFinished() const { return bFinished; }
	/** The agent returned at least one idea. */
	bool Succeeded() const { return bSucceeded; }

	/**
	 * The agent's ideas, in its order. CategoryId / Category are the picked category's doc id and title, or empty for
	 * Uncategorized; Description is the agent's text (AddTasks adds the doc link).
	 */
	const TArray<FProtoBoyNewTask>& GetIdeas() const { return Ideas; }
	/** The picked categories' titles, in map order. */
	TArray<FString> GetCategoryTitles() const;

	/**
	 * Add the ideas at Indices to the board, one at a time. OnProgress runs after each; OnDone once at the end with the
	 * errors for the ones that couldn't be added (empty = all added, or stopped by Cancel).
	 */
	void AddTasks(const TArray<int32>& Indices, TFunction<void()> OnProgress, TFunction<void(const TArray<FText>& Errors)> OnDone);
	bool IsAdding() const { return bAdding; }
	int32 GetNumAdded() const { return NumAdded; }

	const TArray<FString>& GetLog() const { return Agent.GetLog(); }

	/** Saved/ProtoBoy/TaskGeneration/. */
	static FString GetWorkDir();

private:
	/** The agent exited: read the ideas from its answer. */
	void Complete();

	FProtoBoyAgentRun Agent;
	/** A picked category: its doc's id, title and path. */
	struct FPicked
	{
		FString DocId;
		FString Title;
		FString Path;
	};
	/** In map order. */
	TArray<FPicked> Picked;
	TArray<FProtoBoyNewTask> Ideas;
	bool bAdding = false;
	bool bCancelled = false;
	bool bFinished = false;
	bool bSucceeded = false;
	int32 NumAdded = 0;
};
