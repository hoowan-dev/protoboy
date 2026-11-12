#pragma once

#include "CoreMinimal.h"

/**
 * Asks the user to allow or deny an agent's write tool call, with an editor notification (Allow / Deny buttons).
 * Non-blocking: the MCP reply waits for the decision while the editor keeps running. Unanswered requests are
 * denied after a timeout.
 */
class FProtoBoyMCPApprovals
{
public:
	static constexpr float TimeoutSeconds = 120.f;

	/** OnDecision runs once on the game thread with true for Allow. */
	static void Request(const FString& ClientName, const FText& Action, const FString& Details, TFunction<void(bool bApproved)> OnDecision);
};
