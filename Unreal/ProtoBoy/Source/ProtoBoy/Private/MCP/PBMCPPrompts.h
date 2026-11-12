#pragma once

#include "MCP/PBMCPServer.h"

/** ProtoBoy's workflow prompts (plan-feature, implement-task, record-decision). */
namespace ProtoBoyMCPPrompts
{
	void RegisterPrompts(TArray<FProtoBoyMCPPrompt>& OutPrompts);
}
