#pragma once

#include "MCP/PBMCPServer.h"

struct FProtoBoyTask;

/**
 * The task board's MCP tools (tasks_list, task_get, task_create, task_update_status, task_comment) and prompts
 * (generate-tasks, implement-task), added to the central registries.
 */
namespace ProtoBoyTasksMCP
{
	void RegisterTools(TArray<FProtoBoyMCPTool>& OutTools);
	void RegisterPrompts(TArray<FProtoBoyMCPPrompt>& OutPrompts);

	/** "- [id] Title (todo, list "To Do") labels: ... design doc: ...", for task lists (also get_project_context). */
	FString FormatTaskLine(const FProtoBoyTask& Task);
}
