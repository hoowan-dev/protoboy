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
			TEXT("Design a feature against the project's docs: check it serves the Game doc's pillars, find where it belongs, write the design, and break it into tasks. No code yet."),
			{ { TEXT("feature"), TEXT("What the feature should do."), true } },
			[](const TMap<FString, FString>& Args)
			{
				return FString::Printf(TEXT(R"MD(Plan this feature for the project using ProtoBoy. Don't write code yet.

Feature: %s

1. Call `get_project_context` to load the Game doc (pitch, design pillars, non-goals, core loop) and the doc map.
2. Say which design pillar the feature serves. If it serves none, or is one of the non-goals, tell me before going further.
3. Call `locate_feature` with the feature description and decide which Design doc it belongs to (or which shared Tech
   doc). If none fits, propose a new Design doc and, once I agree, record the decision in the Game doc with
   `record_decision`.
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
3. Plan where the code goes from the Tech doc and its Decisions. Don't add a new feature, system or module the docs
   don't cover without my agreement and a `record_decision` entry. Tell me the plan before large changes.
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
			TEXT("Record a design or architecture decision in the Decisions section of the doc it affects."),
			{
				{ TEXT("decision"), TEXT("The decision to record."), true },
				{ TEXT("context"), TEXT("Why it came up (optional)."), false },
			},
			[](const TMap<FString, FString>& Args)
			{
				return FString::Printf(TEXT(R"MD(Record this decision with ProtoBoy.

Decision: %s
Context: %s

1. Call `get_project_context` and find the doc this decision belongs to (`locate_feature`, `docs_read`): the Game doc
   for scope, design pillars and project-wide conventions, otherwise the Design, Tech or Content doc it changes.
2. Read that doc's Decisions section. If an earlier entry conflicts, say whether this one replaces it.
3. Draft the entry: what was decided (one sentence), why, and what it rules out. Show me the draft, and which doc it
   goes in.
4. Once I agree, add it with `record_decision`.
5. If it changes what the doc says elsewhere, propose the exact edit. For the Game doc, only propose it: humans own it.)MD"), *ArgOr(Args, TEXT("decision"), TEXT("")), *ArgOr(Args, TEXT("context"), TEXT("(not given)")));
			}
		});

		// Feature areas register their own prompts.
		ProtoBoySourcingMCP::RegisterPrompts(OutPrompts);
	}
}
