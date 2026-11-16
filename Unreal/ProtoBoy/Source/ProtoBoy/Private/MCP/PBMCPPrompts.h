#pragma once

#include "MCP/PBMCPServer.h"

/** ProtoBoy's workflow prompts: plan-feature here, plus each feature area's (Docs, Tasks, Sourcing). */
namespace ProtoBoyMCPPrompts
{
	void RegisterPrompts(TArray<FProtoBoyMCPPrompt>& OutPrompts);
}
