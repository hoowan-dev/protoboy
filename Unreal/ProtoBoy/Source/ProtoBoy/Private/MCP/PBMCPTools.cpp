#include "MCP/PBMCPTools.h"

#include "Docs/PBDocsService.h"
#include "Docs/PBDocMap.h"
#include "Docs/PBDocsMCP.h"
#include "MCP/PBMCPToolHelpers.h"
#include "PBSubsystem.h"
#include "Sourcing/PBAssetSourcing.h"
#include "Sourcing/PBSourcingMCP.h"
#include "Tasks/PBTaskProvider.h"
#include "Tasks/PBTasksMCP.h"

#define LOCTEXT_NAMESPACE "ProtoBoyMCPTools"

namespace ProtoBoyMCPTools
{
	// -----------------------------------------------------------------------------------------------------------------
	// Subsystem access (declared in MCP/PBMCPToolHelpers.h, with the schema and argument helpers)

	UProtoBoySubsystem* GetSubsystemOrFail(const FProtoBoyMCPToolDone& Done)
	{
		UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
		if (!Subsystem)
		{
			Done(FProtoBoyMCPToolResult::Error(TEXT("ProtoBoy isn't running.")));
		}
		return Subsystem;
	}

	// -----------------------------------------------------------------------------------------------------------------
	// Context

	FString OrNotSet(const FString& Value)
	{
		return Value.IsEmpty() ? FString(TEXT("_Not written yet._")) : Value;
	}

	/** "- [design] Design/Combat/Combat.md: Combat - summary", nested under its parent. */
	void AppendDocTree(const FProtoBoyDocMap& Map, const FProtoBoyDocNode& Node, int32 Depth, FString& Out)
	{
		Out += FString::ChrN(Depth * 2, TEXT(' ')) + FString::Printf(TEXT("- [%s] %s: %s"),
			*ProtoBoyDocLayers::ToKey(Node.Layer), *Node.Path, *Node.Title);
		if (!Node.Summary.IsEmpty())
		{
			Out += FString::Printf(TEXT(" - %s"), *Node.Summary);
		}
		Out += TEXT("\n");
		for (const FProtoBoyDocNode* Child : Map.GetChildren(Node.Id))
		{
			AppendDocTree(Map, *Child, Depth + 1, Out);
		}
	}

	void GetProjectContext(const FProtoBoyMCPToolCall& Call, FProtoBoyMCPToolDone Done)
	{
		UProtoBoySubsystem* Subsystem = GetSubsystemOrFail(Done);
		if (!Subsystem)
		{
			return;
		}

		IProtoBoyDocsProvider& DocsProvider = Subsystem->GetDocsProvider();
		FProtoBoyDocMap& Map = Subsystem->GetDocMap();
		Map.Refresh();
		const FProtoBoyGameDoc Game = Map.ReadGame();

		FString Out = TEXT("# Project context (ProtoBoy)\n\n");
		Out += FString::Printf(TEXT("- **Docs:** %s\n"), *DocsProvider.GetDisplayName().ToString());
		Out += TEXT("- **Doc layers:** Game (what the game is; the root), then Design (what a player should be able to do or "
			"see), Tech (the technical details of how something works) and Content (the assets to make it happen, and the style "
			"they must have), nested however the team likes. Read the docs above and below the one you work in, "
			"and check each doc's Decisions section.\n");

		if (!Game.bExists)
		{
			Out += TEXT("\n**There is no Game doc yet.** Without it you can't tell whether new work serves the game. "
				"Ask the user to set it up (ProtoBoy dashboard > Documentation > Doc Map), or offer to draft it for review.\n");
		}
		else
		{
			Out += FString::Printf(TEXT("- **Game doc:** %s (read it in full with docs_read)\n"), *Game.Path);
			Out += FString::Printf(TEXT("\n## Pitch\n\n%s\n"), *OrNotSet(Game.Pitch));
			Out += TEXT("\n## Design pillars\n\nEvery feature should serve at least one. If a change serves none, say so before building it.\n\n");
			if (Game.Pillars.IsEmpty())
			{
				Out += TEXT("_None written yet._\n");
			}
			for (const FProtoBoyDesignPillar& Pillar : Game.Pillars)
			{
				Out += FString::Printf(TEXT("### %s\n\n%s\n\n"), *Pillar.Name, *OrNotSet(Pillar.Text));
			}
			Out += FString::Printf(TEXT("\n## Non-goals\n\n%s\n"), *OrNotSet(Game.NonGoals));
			Out += FString::Printf(TEXT("\n## Core loop\n\n%s\n"), *OrNotSet(Game.CoreLoop));
		}

		Out += FString::Printf(TEXT("\n## Asset sourcing\n\n%s"), *ProtoBoyAssetSourcing::DescribePolicyForAgents());

		Out += TEXT("\n## Doc map\n\n");
		if (const FProtoBoyDocNode* Root = Map.GetGame())
		{
			AppendDocTree(Map, *Root, 0, Out);
		}
		else
		{
			Out += TEXT("_No map yet (no Game doc)._\n");
		}
		if (!Map.GetUnmappedDocs().IsEmpty())
		{
			Out += FString::Printf(TEXT("\n%d other docs aren't on the map (no layer); find them with docs_list or docs_search.\n"), Map.GetUnmappedDocs().Num());
		}

		// Task categories: where new tasks go (task_create's category).
		Out += TEXT("\n## Task categories\n\nEach has a \"To Do - <Category>\" list on the board and a label of its name. Pass the category to task_create; tasks that fit none go to Uncategorized.\n\n");
		const TArray<const FProtoBoyDocNode*> Categories = Map.GetTaskCategories();
		for (const FProtoBoyDocNode* Category : Categories)
		{
			Out += FString::Printf(TEXT("- **%s** (%s)\n"), *Category->Title, *Category->Path);
		}
		if (Categories.IsEmpty())
		{
			Out += TEXT("_None yet: the team marks docs as task categories on ProtoBoy's doc map._\n");
		}

		// Tasks are optional: report why they're missing instead of failing.
		IProtoBoyTaskProvider* Tasks = Subsystem->GetTaskProvider();
		FText WhyNot = LOCTEXT("ContextNoBoard", "No task board is connected.");
		if (!Tasks || !Tasks->IsReady(WhyNot))
		{
			Out += FString::Printf(TEXT("\n## Tasks\n\n_%s_\n"), *WhyNot.ToString());
			Done(FProtoBoyMCPToolResult::Ok(Out));
			return;
		}

		const FString BoardName = Tasks->GetDisplayName().ToString();
		Tasks->ListTasks({}, [Out, BoardName, Done = MoveTemp(Done)](const TProtoBoyTaskResult<TArray<FProtoBoyTask>>& Result) mutable
		{
			Out += FString::Printf(TEXT("\n## Tasks (%s)\n\n"), *BoardName);
			if (!Result.bOk)
			{
				Out += FString::Printf(TEXT("_Couldn't load tasks: %s_\n"), *Result.Error.ToString());
			}
			else
			{
				for (const EProtoBoyTaskStatus Status : { EProtoBoyTaskStatus::InProgress, EProtoBoyTaskStatus::ToDo })
				{
					Out += Status == EProtoBoyTaskStatus::InProgress ? TEXT("### In progress\n\n") : TEXT("\n### To do (first 20)\n\n");
					int32 Shown = 0;
					for (const FProtoBoyTask& Task : Result.Value)
					{
						if (Task.Status == Status && Shown < 20)
						{
							Out += ProtoBoyTasksMCP::FormatTaskLine(Task) + TEXT("\n");
							++Shown;
						}
					}
					if (Shown == 0)
					{
						Out += TEXT("_None._\n");
					}
				}
			}
			Done(FProtoBoyMCPToolResult::Ok(Out));
		});
	}

	// -----------------------------------------------------------------------------------------------------------------
	// Registration

	void RegisterTools(TArray<FProtoBoyMCPTool>& OutTools)
	{
		OutTools.Reset();
		OutTools.Add(MakeTool(TEXT("get_project_context"), LOCTEXT("ContextTitle", "Get project context"),
			LOCTEXT("ContextDesc", "Start here. Returns the Game doc (pitch, design pillars, non-goals, core loop), the doc map (every doc by layer), the task categories and open tasks."),
			false, {}, &GetProjectContext));

		// Feature areas register their own tools.
		ProtoBoyDocsMCP::RegisterTools(OutTools);
		ProtoBoyTasksMCP::RegisterTools(OutTools);
		ProtoBoySourcingMCP::RegisterTools(OutTools);
	}
}

#undef LOCTEXT_NAMESPACE
