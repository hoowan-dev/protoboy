#pragma once

#include "CoreMinimal.h"
#include "Health/PBDocHealthReport.h"
#include "Health/PBDocHealthRun.h"
#include "Health/PBDocStructure.h"
#include "MCP/PBMCPConnection.h"

class FProtoBoyDocMap;

/**
 * The documentation health checks as the editor last saw them, owned by the subsystem (GetDocHealth()) so they keep
 * running and stay readable whether or not the dashboard is open. The Health page and the status bar hub are both
 * views of this:
 * - the check against revision control: the current or last FProtoBoyDocHealthRun (ticked by the subsystem), how it
 *   ended, and the last report (this session's, else the one saved by an earlier session);
 * - the structure check: the last ProtoBoyDocStructure::Check result, and whether the docs changed since.
 * OnChanged fires when any of it changes (not for new log lines: read GetLog when drawing).
 */
class PROTOBOY_API FProtoBoyDocHealthService
{
public:
	explicit FProtoBoyDocHealthService(FProtoBoyDocMap& InDocMap);
	~FProtoBoyDocHealthService();

	/** The agent checks run with: the one picked this session, else the first set-up agent that can run unattended. Unset if there's none. */
	TOptional<EProtoBoyMCPClient> GetAgent() const;
	void SetAgent(EProtoBoyMCPClient Agent);

	/** Start a check against revision control with GetAgent(). False with a reason if it couldn't start (also kept as GetRunStatus). */
	bool StartCheck(FText& OutError);
	/** Stop the agent. The last report stays. */
	void CancelCheck();
	bool IsChecking() const;
	/** The agent running the current check (meaningful while IsChecking). */
	EProtoBoyMCPClient GetCheckingAgent() const;
	/** Read the agent's output and, once it's done, take its report. Called by the subsystem every tick. */
	void Tick();

	/** Goes up with every check started, so views know when to clear their log. */
	int32 GetRunSerial() const { return RunSerial; }
	/** The current or last check's progress lines; empty before the first check. */
	const TArray<FString>& GetLog() const;
	/** How the last check ended, or why it couldn't start. Empty while one runs. */
	const FText& GetRunStatus() const { return RunStatus; }
	bool DidRunFail() const { return bRunFailed; }

	bool HasReport() const { return bHasReport; }
	const FProtoBoyDocHealthReport& GetReport() const { return Report; }
	/** One line on the last report: when, with which agent, how many docs may need updating. Empty without a report. */
	FText DescribeReport() const;
	/** The doc is fine as it is: stamp it as reviewed (FProtoBoyDocMap::MarkReviewed) and take it off the report. */
	bool MarkUpToDate(const FString& DocId, FText& OutError);

	/** Run the structure checks now (reads every doc). */
	void CheckStructure();
	bool IsStructureChecked() const { return bStructureChecked; }
	/** The docs changed since the last structure check, or it never ran. */
	bool IsStructureStale() const { return bStructureStale; }
	const TArray<FProtoBoyDocIssue>& GetIssues() const { return Issues; }
	/** The issues that are warnings (broken references), not just things to fill in. */
	int32 CountProblems() const;

	FSimpleMulticastDelegate OnChanged;

private:
	/** The run just finished: take its report and say how it ended. */
	void HandleRunFinished();

	FProtoBoyDocMap& DocMap;
	FDelegateHandle DocMapChangedHandle;
	TOptional<EProtoBoyMCPClient> PickedAgent;

	TUniquePtr<FProtoBoyDocHealthRun> Run;
	/** The run's end has been handled (HandleRunFinished). */
	bool bRunHandled = true;
	int32 RunSerial = 0;
	FText RunStatus;
	bool bRunFailed = false;

	FProtoBoyDocHealthReport Report;
	bool bHasReport = false;

	TArray<FProtoBoyDocIssue> Issues;
	bool bStructureChecked = false;
	bool bStructureStale = true;
};
