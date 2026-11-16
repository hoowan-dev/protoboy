#pragma once

#include "MCP/PBMCPServer.h"

/**
 * ProtoBoy's MCP tool registry: get_project_context here, plus each feature area's tools (ProtoBoyDocsMCP,
 * ProtoBoyTasksMCP, ProtoBoySourcingMCP). Handlers go through UProtoBoySubsystem's providers, so they work with
 * whichever docs/task platform is connected.
 */
namespace ProtoBoyMCPTools
{
	void RegisterTools(TArray<FProtoBoyMCPTool>& OutTools);
}
