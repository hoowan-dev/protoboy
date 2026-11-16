#pragma once

#include "MCP/PBMCPServer.h"

/**
 * The docs' MCP tools (locate_feature, docs_list, docs_search, docs_read, docs_create, docs_update_section,
 * record_decision) and prompts (record-decision), added to the central registries.
 */
namespace ProtoBoyDocsMCP
{
	void RegisterTools(TArray<FProtoBoyMCPTool>& OutTools);
	void RegisterPrompts(TArray<FProtoBoyMCPPrompt>& OutPrompts);
}
