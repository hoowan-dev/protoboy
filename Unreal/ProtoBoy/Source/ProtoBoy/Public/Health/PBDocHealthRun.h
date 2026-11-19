#pragma once

#include "CoreMinimal.h"
#include "Agents/PBAgentRun.h"
#include "Health/PBDocHealthChanges.h"
#include "Health/PBDocHealthReport.h"

/**
 * One documentation health check (Health page > Run Health Checks), in two stages:
 * 1. ProtoBoyDocHealthChanges::Collect lists the project files changed since each doc on the map was last edited, and
 *    which of them each doc names. If nothing changed, the check ends there with an empty report and no agent runs.
 * 2. One of the user's agents (FProtoBoyAgentRun) gets each of those docs' text and the changed files, may read the
 *    files, and answers with the docs that may need updating: {"docs": [{id, confidence, reason, files}]}.
 *
 * If the agent gives no usable answer, the report falls back to the docs that name changed files, marked as unjudged
 * (FProtoBoyDocHealthReport::bAgentAnswered). The report is saved with ProtoBoyDocHealthReport::Save; a cancelled run
 * saves nothing. Tick() is polled by the page. The prompt and raw output are kept in Saved/ProtoBoy/DocHealth/.
 */
class PROTOBOY_API FProtoBoyDocHealthRun
{
public:
	~FProtoBoyDocHealthRun();

	/** Collect the changes and start the agent. False (nothing started) with a reason. Use a new run for each check. */
	bool Start(EProtoBoyMCPClient InClient, FText& OutError);
	/** Read the agent's output, and once it exits, build and save the report. Call often. */
	void Tick();
	/** Stop the agent; no report. */
	void Cancel();

	bool IsRunning() const { return Agent.IsRunning(); }
	bool IsFinished() const { return bFinished; }
	/** There's a report (possibly the fallback one, or an empty one). */
	bool Succeeded() const { return bSucceeded; }
	bool WasCancelled() const { return bCancelled; }
	EProtoBoyMCPClient GetClient() const { return Client; }

	const FProtoBoyDocHealthReport& GetReport() const { return Report; }
	const TArray<FString>& GetLog() const { return Agent.GetLog(); }

	/** Saved/ProtoBoy/DocHealth/. */
	static FString GetWorkDir();

private:
	/** The agent exited: read the stale docs from its answer, or fall back to the docs that name changed files. */
	void Complete();
	/** Stamp the report, save it and mark the run done. */
	void Finish();

	FProtoBoyAgentRun Agent;
	EProtoBoyMCPClient Client = EProtoBoyMCPClient::ClaudeCode;
	FProtoBoyDocHealthChanges Changes;
	FProtoBoyDocHealthReport Report;
	bool bFinished = false;
	bool bSucceeded = false;
	bool bCancelled = false;
};
