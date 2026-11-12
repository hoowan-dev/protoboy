#pragma once

#include "MCP/PBMCPServer.h"

/** Asset sourcing's MCP tools (fab_search) and prompts (source-assets), added to the central registries. */
namespace ProtoBoySourcingMCP
{
	void RegisterTools(TArray<FProtoBoyMCPTool>& OutTools);
	void RegisterPrompts(TArray<FProtoBoyMCPPrompt>& OutPrompts);
}
