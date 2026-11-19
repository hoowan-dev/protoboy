#include "Health/PBDocHealthService.h"

#include "Agents/PBAgentRun.h"
#include "Docs/PBDocMap.h"

#define LOCTEXT_NAMESPACE "ProtoBoyDocHealthService"

FProtoBoyDocHealthService::FProtoBoyDocHealthService(FProtoBoyDocMap& InDocMap)
	: DocMap(InDocMap)
{
	bHasReport = ProtoBoyDocHealthReport::Load(Report);
	// No broadcast: views check the structure again when they're next on screen (it reads every doc).
	DocMapChangedHandle = DocMap.OnChanged.AddLambda([this]() { bStructureStale = true; });
}

FProtoBoyDocHealthService::~FProtoBoyDocHealthService()
{
	DocMap.OnChanged.Remove(DocMapChangedHandle);
}

TOptional<EProtoBoyMCPClient> FProtoBoyDocHealthService::GetAgent() const
{
	if (PickedAgent.IsSet())
	{
		return PickedAgent;
	}
	for (const EProtoBoyMCPClient Agent : FProtoBoyAgentRun::GetSetUpAgents())
	{
		FText Ignored;
		if (FProtoBoyAgentRun::CanRun(Agent, Ignored))
		{
			return Agent;
		}
	}
	return TOptional<EProtoBoyMCPClient>();
}

void FProtoBoyDocHealthService::SetAgent(EProtoBoyMCPClient Agent)
{
	PickedAgent = Agent;
}

bool FProtoBoyDocHealthService::StartCheck(FText& OutError)
{
	if (IsChecking())
	{
		OutError = LOCTEXT("AlreadyChecking", "A health check is already running.");
		return false;
	}

	const TOptional<EProtoBoyMCPClient> Agent = GetAgent();
	TUniquePtr<FProtoBoyDocHealthRun> NewRun = MakeUnique<FProtoBoyDocHealthRun>();
	if (!Agent.IsSet())
	{
		OutError = LOCTEXT("NoAgent", "No agent is set up to check the docs. Connect Claude Code or Gemini CLI in Setup > MCP.");
	}
	if (!Agent.IsSet() || !NewRun->Start(Agent.GetValue(), OutError))
	{
		RunStatus = OutError;
		bRunFailed = true;
		OnChanged.Broadcast();
		return false;
	}

	Run = MoveTemp(NewRun);
	bRunHandled = false;
	++RunSerial;
	RunStatus = FText::GetEmpty();
	bRunFailed = false;
	OnChanged.Broadcast();

	// Nothing had changed since the docs were edited: the run is done without an agent.
	if (Run->IsFinished())
	{
		HandleRunFinished();
	}
	return true;
}

void FProtoBoyDocHealthService::CancelCheck()
{
	if (IsChecking())
	{
		Run->Cancel();
		HandleRunFinished();
	}
}

bool FProtoBoyDocHealthService::IsChecking() const
{
	return Run.IsValid() && !Run->IsFinished();
}

EProtoBoyMCPClient FProtoBoyDocHealthService::GetCheckingAgent() const
{
	return Run.IsValid() ? Run->GetClient() : EProtoBoyMCPClient::ClaudeCode;
}

void FProtoBoyDocHealthService::Tick()
{
	if (Run.IsValid() && !bRunHandled)
	{
		Run->Tick();
		if (Run->IsFinished())
		{
			HandleRunFinished();
		}
	}
}

void FProtoBoyDocHealthService::HandleRunFinished()
{
	bRunHandled = true;
	if (Run->Succeeded())
	{
		Report = Run->GetReport();
		bHasReport = true;
		bRunFailed = !Report.bAgentAnswered;
		RunStatus = Report.bAgentAnswered
			? LOCTEXT("RunDone", "Done. See Docs That May Need Updating on the Health page.")
			: LOCTEXT("RunNoAnswer", "The agent didn't give an answer. See the Agent Log on the Health page for why.");
	}
	else
	{
		bRunFailed = false;
		RunStatus = LOCTEXT("RunCancelled", "Cancelled.");
	}
	OnChanged.Broadcast();
}

const TArray<FString>& FProtoBoyDocHealthService::GetLog() const
{
	static const TArray<FString> Empty;
	return Run.IsValid() ? Run->GetLog() : Empty;
}

FText FProtoBoyDocHealthService::DescribeReport() const
{
	if (!bHasReport)
	{
		return FText::GetEmpty();
	}
	// AsDateTime takes UTC and shows it in the user's time zone.
	const FText When = FText::AsDateTime(Report.CheckedUtc, EDateTimeStyle::Medium, EDateTimeStyle::Short);
	if (Report.AgentName.IsEmpty())
	{
		return FText::Format(LOCTEXT("SummaryNoChanges", "Last checked {0}: no project files had changed since the {1} {1}|plural(one=doc was,other=docs were) last edited."),
			When, Report.NumDocsChecked);
	}
	if (Report.Docs.IsEmpty())
	{
		return FText::Format(LOCTEXT("SummaryNone", "Last checked {0} with {1}: none of the {2} {2}|plural(one=doc,other=docs) {2}|plural(one=looks,other=look) out of date ({3} changed {3}|plural(one=file,other=files))."),
			When, FText::FromString(Report.AgentName), Report.NumDocsChecked, Report.NumChangedFiles);
	}
	return FText::Format(LOCTEXT("SummarySome", "Last checked {0} with {1}: {2} of {3} {3}|plural(one=doc,other=docs) may need updating ({4} changed {4}|plural(one=file,other=files))."),
		When, FText::FromString(Report.AgentName), Report.Docs.Num(), Report.NumDocsChecked, Report.NumChangedFiles);
}

bool FProtoBoyDocHealthService::MarkUpToDate(const FString& InDocId, FText& OutError)
{
	// Copied: the views rebuild on OnChanged, which may destroy whatever InDocId came from.
	const FString DocId = InDocId;
	if (!DocMap.MarkReviewed(DocId, OutError))
	{
		return false;
	}
	Report.Docs.RemoveAll([&DocId](const FProtoBoyStaleDoc& Doc) { return Doc.DocId == DocId; });
	ProtoBoyDocHealthReport::Save(Report);
	OnChanged.Broadcast();
	return true;
}

void FProtoBoyDocHealthService::CheckStructure()
{
	Issues = ProtoBoyDocStructure::Check();
	bStructureChecked = true;
	bStructureStale = false;
	OnChanged.Broadcast();
}

int32 FProtoBoyDocHealthService::CountProblems() const
{
	int32 Num = 0;
	for (const FProtoBoyDocIssue& Issue : Issues)
	{
		Num += Issue.bWarning ? 1 : 0;
	}
	return Num;
}

#undef LOCTEXT_NAMESPACE
