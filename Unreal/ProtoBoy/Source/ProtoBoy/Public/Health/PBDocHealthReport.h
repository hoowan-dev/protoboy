#pragma once

#include "CoreMinimal.h"

/** A doc the health check thinks may need updating. */
struct FProtoBoyStaleDoc
{
	FString DocId;
	/** Relative to the docs root. */
	FString Path;
	FString Title;
	/** The agent's "high", "medium" or "low"; empty when the agent didn't answer and the doc is only listed for naming changed files. */
	FString Confidence;
	/** The agent's one or two sentences on what in the doc the changes may have outdated. */
	FString Reason;
	/** The changed files behind it, relative to the project folder, most important first. */
	TArray<FString> Files;
	/** When the doc was last edited, as of the check. */
	FDateTime DocModifiedUtc;
};

/** The result of one documentation health check (FProtoBoyDocHealthRun). */
struct FProtoBoyDocHealthReport
{
	FDateTime CheckedUtc;
	/** The agent that judged the docs, e.g. "Claude Code"; empty when nothing had changed, so none ran. */
	FString AgentName;
	/** Docs on the map that were looked at. */
	int32 NumDocsChecked = 0;
	/** Project files changed since the oldest doc was edited. */
	int32 NumChangedFiles = 0;
	/** False when the agent gave no usable answer: Docs then lists the docs that name changed files, unjudged. */
	bool bAgentAnswered = true;
	/** Most confident first. */
	TArray<FProtoBoyStaleDoc> Docs;
};

/**
 * The last health check's report, kept per user in <Project>/Saved/ProtoBoy/DocHealth/LastReport.json so the Health
 * page can show it again after a restart. Nothing is written into the docs themselves.
 */
namespace ProtoBoyDocHealthReport
{
	PROTOBOY_API FString GetFilePath();
	/** False if there's no report yet, or it can't be read. */
	PROTOBOY_API bool Load(FProtoBoyDocHealthReport& Out);
	PROTOBOY_API bool Save(const FProtoBoyDocHealthReport& Report);
}
