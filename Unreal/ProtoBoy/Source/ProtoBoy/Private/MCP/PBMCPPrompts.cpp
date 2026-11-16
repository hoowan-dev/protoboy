#include "MCP/PBMCPPrompts.h"

#include "Docs/PBDocsMCP.h"
#include "MCP/PBMCPToolHelpers.h"
#include "Sourcing/PBSourcingMCP.h"
#include "Tasks/PBTasksMCP.h"

namespace ProtoBoyMCPPrompts
{
	void RegisterPrompts(TArray<FProtoBoyMCPPrompt>& OutPrompts)
	{
		OutPrompts.Reset();

		OutPrompts.Add({
			TEXT("plan-feature"),
			TEXT("Plan a feature"),
			TEXT("Design a feature against the project's docs: check it serves the Game doc's pillars, find where it belongs, write the design, and break it into tasks. No code yet."),
			{ { TEXT("feature"), TEXT("What the feature should do."), true } },
			[](const TMap<FString, FString>& Args)
			{
				return FString::Printf(TEXT(R"MD(Plan this feature for the project using ProtoBoy. Don't write code yet.

Feature: %s

1. Call `get_project_context` to load the Game doc (pitch, design pillars, non-goals, core loop) and the doc map.
2. Say which design pillar the feature serves. If it serves none, or is one of the non-goals, tell me before going further.
3. Call `locate_feature` with the feature description and decide which Design doc it belongs to (or which Tech doc).
   If none fits, propose a new Design doc and, once I agree, record the decision in the Game doc with
   `record_decision`.
4. Write the design from the player's side in that Design doc (`docs_update_section`, mode "append"): purpose,
   what the player can do, what the player sees, rules, edge cases, open questions. If it's a new feature, ask me to add its
   Design doc from ProtoBoy's Doc Map first, so it starts from the template and lands on the map.
5. Write how it will work in a Tech doc for it (usually under the Design doc on the map; ask me to add one if there
   isn't one): how it works, code, data, dependencies, constraints. List the assets it needs in a Content doc, with the
   style they must have.
6. Break the work into tasks small enough for one session each. Show them to me; after I approve, create them with
   `task_create` (status "todo") and link the Design doc in each.
7. Summarize: the pillar it serves, the Design and Tech doc paths, tasks created, open questions.)MD"), *ProtoBoyMCPTools::ArgOr(Args, TEXT("feature"), TEXT("")));
			}
		});

		// Feature areas register their own prompts.
		// Feature areas register their own prompts.
		ProtoBoyDocsMCP::RegisterPrompts(OutPrompts);
		ProtoBoyTasksMCP::RegisterPrompts(OutPrompts);
		ProtoBoySourcingMCP::RegisterPrompts(OutPrompts);
	}
}
