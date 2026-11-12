#pragma once

#include "CoreMinimal.h"

/**
 * Audit trail of what agents changed through ProtoBoy: one JSON line per write tool call (including denied ones),
 * in <Project>/Saved/ProtoBoy/AgentActivity.jsonl. Per user, not committed.
 */
class FProtoBoyAgentActivityLog
{
public:
	static FString GetFilePath();

	static void Append(const FString& ClientName, const FString& ToolName, const FString& ArgsSummary, const FString& Outcome);
};
