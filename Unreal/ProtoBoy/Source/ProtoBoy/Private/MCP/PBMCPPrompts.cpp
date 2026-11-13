#include "MCP/PBMCPPrompts.h"

#include "Sourcing/PBSourcingMCP.h"

namespace ProtoBoyMCPPrompts
{
	FString ArgOr(const TMap<FString, FString>& Args, const TCHAR* Name, const TCHAR* Fallback)
	{
		const FString Value = Args.FindRef(Name).TrimStartAndEnd();
		return Value.IsEmpty() ? FString(Fallback) : Value;
	}

	void RegisterPrompts(TArray<FProtoBoyMCPPrompt>& OutPrompts)
	{
		OutPrompts.Reset();

		OutPrompts.Add({
			TEXT("plan-feature"),
			TEXT("Plan a feature"),
			TEXT("Design a feature against the project's docs: check it serves the Vision, find where it belongs, write the design, and break it into tasks. No code yet."),
			{ { TEXT("feature"), TEXT("What the feature should do."), true } },
			[](const TMap<FString, FString>& Args)
			{
				return FString::Printf(TEXT(R"MD(Plan this feature for the project using ProtoBoy. Don't write code yet.

Feature: %s

1. Call `get_project_context` to load the Vision (pitch, design pillars, non-goals), the Overview and the doc map.
2. Say which design pillar the feature serves. If it serves none, or is one of the non-goals, tell me before going further.
3. Call `locate_feature` with the feature description and decide which Design doc it belongs to (or which Systems doc,
   for shared tech). If none fits, propose a new Design doc and, once I agree, record the decision with `adr_create`.
4. Write the design from the player's side in that Design doc (`docs_update_section`, mode "append"): purpose,
   player experience, rules, what the player sees, edge cases, open questions. If it's a new feature, ask me to add its
   Design doc from ProtoBoy's Doc Map first, so it starts from the template and lands on the map.
5. Write how it will be built in the Design doc's Tech doc (ask me to add one with Add Tech Doc if it has none): one
   section per Design section, covering classes and modules, data and tunables, interfaces, constraints.
6. Break the work into tasks small enough for one session each. Show them to me; after I approve, create them with
   `task_create` (status "todo") and link the Design doc in each.
7. Summarize: the pillar it serves, the Design and Tech doc paths, tasks created, open questions.)MD"), *ArgOr(Args, TEXT("feature"), TEXT("")));
			}
		});

		OutPrompts.Add({
			TEXT("implement-task"),
			TEXT("Implement a task"),
			TEXT("Implement a task from the board as its Design and Tech docs describe, then update the task and docs."),
			{ { TEXT("task_id"), TEXT("The task's id (Trello card short link or id)."), true } },
			[](const TMap<FString, FString>& Args)
			{
				return FString::Printf(TEXT(R"MD(Implement task %s using ProtoBoy to build what the project's docs describe.

1. Call `task_get` for the task's description, comments and linked Design doc.
2. Call `get_project_context`, then `docs_read` the linked Design doc and its Tech doc.
   Use `locate_feature` if the task links no doc.
3. Plan where the code goes from the Tech doc. Don't add a new feature, system or module the docs don't cover without
   an ADR (`adr_create`) and my agreement. Tell me the plan before large changes.
4. Move the task to in progress with `task_update_status`.
5. Implement in small steps inside the owning system. Keep to the task's scope; put anything extra into new tasks
   (`task_create`) instead of growing this one.
6. When finished:
   - Record how it was built in the Tech doc, and any change to the design in the Design doc
     (`docs_update_section`, mode "append").
   - Comment on the task with a summary of the changes and follow-ups (`task_comment`).
   - Move it to done with `task_update_status` only if the acceptance criteria are met; otherwise leave it in progress
     and say what's left.)MD"), *ArgOr(Args, TEXT("task_id"), TEXT("")));
			}
		});

		OutPrompts.Add({
			TEXT("record-decision"),
			TEXT("Record a decision"),
			TEXT("Write an Architecture Decision Record for a design or architecture choice."),
			{
				{ TEXT("decision"), TEXT("The decision to record."), true },
				{ TEXT("context"), TEXT("Why it came up (optional)."), false },
			},
			[](const TMap<FString, FString>& Args)
			{
				return FString::Printf(TEXT(R"MD(Record this architecture decision with ProtoBoy.

Decision: %s
Context: %s

1. Call `get_project_context` and read the docs this decision affects (`locate_feature`, `docs_read`).
2. Search for related ADRs (`docs_search`, the ADR folder). If one exists, say whether this one extends or supersedes it.
3. Draft the context, the decision, its consequences (including what becomes harder), and the alternatives considered.
   Show me the draft.
4. Once I agree, create it with `adr_create` (status "Proposed", or "Accepted" if I say so).
5. If it changes a Design, Tech or Systems doc, propose the exact changes. If it touches the Vision or Overview, only
   propose them: humans own those docs.)MD"), *ArgOr(Args, TEXT("decision"), TEXT("")), *ArgOr(Args, TEXT("context"), TEXT("(not given)")));
			}
		});

		// Feature areas register their own prompts.
		ProtoBoySourcingMCP::RegisterPrompts(OutPrompts);
	}
}
