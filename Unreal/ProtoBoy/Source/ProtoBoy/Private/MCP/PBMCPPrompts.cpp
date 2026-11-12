#include "MCP/PBMCPPrompts.h"

#include "Docs/PBDocGenerator.h"

namespace ProtoBoyMCPPrompts
{
	FString ArgOr(const TMap<FString, FString>& Args, const TCHAR* Name, const TCHAR* Fallback)
	{
		const FString Value = Args.FindRef(Name).TrimStartAndEnd();
		return Value.IsEmpty() ? FString(Fallback) : Value;
	}

	/** Fill in the configured Architecture doc path. */
	FString WithArchitecturePath(const FString& Text)
	{
		return Text.Replace(TEXT("{ARCHITECTURE_DOC}"), *FProtoBoyDocGenerator::GetArchitectureDocPath());
	}

	void RegisterPrompts(TArray<FProtoBoyMCPPrompt>& OutPrompts)
	{
		OutPrompts.Reset();

		OutPrompts.Add({
			TEXT("plan-feature"),
			TEXT("Plan a feature"),
			TEXT("Design a feature within the project's architecture: find the owning system, write the design, and break it into tasks. No code yet."),
			{ { TEXT("feature"), TEXT("What the feature should do."), true } },
			[](const TMap<FString, FString>& Args)
			{
				return WithArchitecturePath(FString::Printf(TEXT(R"MD(Plan this feature for the project using ProtoBoy. Don't write code yet.

Feature: %s

1. Call `get_project_context` to load the end goal, current milestone, systems and rules.
2. Call `locate_feature` with the feature description and decide which existing system owns it.
   If none fits, stop and explain why; propose a new system and, once I agree, record it with `adr_create`.
3. Check the plan against the Dependency Rules and Conventions. Say how the feature serves the end goal and the
   current milestone. If it doesn't, tell me before going further.
4. Look for existing design docs (`docs_search`, `docs_read`). Write the design either by appending to the owning
   system's design doc (`docs_update_section`, mode "append") or as a new `Design/<Feature>.md` (`docs_create`).
   Cover: goal, player-facing behavior, where the code lives, data and interfaces, risks, acceptance criteria.
5. Break the work into tasks small enough for one session each. Show them to me; after I approve, create them with
   `task_create` (status "todo") and link the design doc in each.
6. Summarize: owning system, design doc path, tasks created, open questions.)MD"), *ArgOr(Args, TEXT("feature"), TEXT(""))));
			}
		});

		OutPrompts.Add({
			TEXT("implement-task"),
			TEXT("Implement a task"),
			TEXT("Implement a task from the board inside the system that owns it, then update the task and design doc."),
			{ { TEXT("task_id"), TEXT("The task's id (Trello card short link or id)."), true } },
			[](const TMap<FString, FString>& Args)
			{
				return WithArchitecturePath(FString::Printf(TEXT(R"MD(Implement task %s using ProtoBoy to stay within the project's architecture.

1. Call `task_get` for the task's description, comments and linked design doc.
2. Call `get_project_context`, then `docs_read` the linked design doc and the owning system in `{ARCHITECTURE_DOC}`.
   Use `locate_feature` if the owning system isn't obvious.
3. Plan where the code goes, following the Dependency Rules and Conventions. Don't add a new system or module, or
   break a rule, without an ADR (`adr_create`) and my agreement. Tell me the plan before large changes.
4. Move the task to in progress with `task_update_status`.
5. Implement in small steps inside the owning system. Keep to the task's scope; put anything extra into new tasks
   (`task_create`) instead of growing this one.
6. When finished:
   - Append what was built and any deviations to the design doc (`docs_update_section`, mode "append").
   - Comment on the task with a summary of the changes and follow-ups (`task_comment`).
   - Move it to done with `task_update_status` only if the acceptance criteria are met; otherwise leave it in progress
     and say what's left.)MD"), *ArgOr(Args, TEXT("task_id"), TEXT(""))));
			}
		});

		OutPrompts.Add({
			TEXT("source-assets"),
			TEXT("Source assets"),
			TEXT("Find assets for a need from the project's enabled sources (e.g. Fab) that fit the design and art direction, instead of generating them."),
			{ { TEXT("need"), TEXT("What the assets are for, e.g. \"props for the tavern level\"."), true } },
			[](const TMap<FString, FString>& Args)
			{
				return WithArchitecturePath(FString::Printf(TEXT(R"MD(Find assets for this need using ProtoBoy's asset sourcing. Don't generate assets with AI.

Need: %s

1. Call `get_project_context` and read its Asset sourcing section: the policy, the enabled sources, and the art
   direction doc. Read the art direction doc (`docs_read`) and the design doc for this feature, if there is one.
2. Write down what fits: style, scale, level of detail or poly budget, palette, mood, license, and anything to avoid.
   Check it against the art direction; if they conflict, ask me.
3. If Fab is enabled, call `fab_search` with search terms built from that (one search per distinct asset type). You
   only get the URL, not results: tell me what to look for on each search.
4. Ask which listings I pick. Record them in the design doc (`docs_update_section`, mode "append"): name, link, what
   each is for, license.
5. Where nothing fits, propose a clearly marked placeholder and a follow-up task (`task_create`), never an
   AI-generated asset.)MD"), *ArgOr(Args, TEXT("need"), TEXT(""))));
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
				return WithArchitecturePath(FString::Printf(TEXT(R"MD(Record this architecture decision with ProtoBoy.

Decision: %s
Context: %s

1. Call `get_project_context` and read the parts of `{ARCHITECTURE_DOC}` this decision affects.
2. Search for related ADRs (`docs_search`, the ADR folder). If one exists, say whether this one extends or supersedes it.
3. Draft the context, the decision, its consequences (including what becomes harder), and the alternatives considered.
   Show me the draft.
4. Once I agree, create it with `adr_create` (status "Proposed", or "Accepted" if I say so).
5. If it changes systems or rules, propose the exact `{ARCHITECTURE_DOC}` changes. Humans own that file: only append
   with `docs_update_section` if I ask you to.)MD"), *ArgOr(Args, TEXT("decision"), TEXT("")), *ArgOr(Args, TEXT("context"), TEXT("(not given)"))));
			}
		});
	}
}
