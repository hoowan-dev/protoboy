#pragma once

#include "MCP/PBMCPServer.h"

/**
 * ProtoBoy's MCP tools: project context, architecture placement, docs and tasks.
 * Handlers go through UProtoBoySubsystem's providers, so they work with whichever docs/task platform is connected.
 */
namespace ProtoBoyMCPTools
{
	void RegisterTools(TArray<FProtoBoyMCPTool>& OutTools);
}
