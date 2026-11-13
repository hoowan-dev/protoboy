#include "MCP/PBMCPTools.h"

#include "Docs/PBDocGenerator.h"
#include "Docs/PBDocMap.h"
#include "Docs/PBMarkdown.h"
#include "Dom/JsonObject.h"
#include "MCP/PBMCPToolHelpers.h"
#include "PBSubsystem.h"
#include "Sourcing/PBAssetSourcing.h"
#include "Sourcing/PBSourcingMCP.h"
#include "Tasks/PBTaskProvider.h"

#define LOCTEXT_NAMESPACE "ProtoBoyMCPTools"

namespace ProtoBoyMCPTools
{
	// -----------------------------------------------------------------------------------------------------------------
	// Subsystem access (schema and argument helpers are in MCP/PBMCPToolHelpers.h)

	UProtoBoySubsystem* GetSubsystemOrFail(const FProtoBoyMCPToolDone& Done)
	{
		UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
		if (!Subsystem)
		{
			Done(FProtoBoyMCPToolResult::Error(TEXT("ProtoBoy isn't running.")));
		}
		return Subsystem;
	}

	/** The task board, or null after finishing the call with the reason it can't be used. */
	IProtoBoyTaskProvider* GetTasksOrFail(const FProtoBoyMCPToolDone& Done)
	{
		UProtoBoySubsystem* Subsystem = GetSubsystemOrFail(Done);
		if (!Subsystem)
		{
			return nullptr;
		}
		IProtoBoyTaskProvider* Tasks = Subsystem->GetTaskProvider();
		FText WhyNot = LOCTEXT("NoBoard", "No task board is connected. Ask the user to connect one in ProtoBoy > Connect Task Board.");
		if (!Tasks || !Tasks->IsReady(WhyNot))
		{
			Done(FProtoBoyMCPToolResult::Error(WhyNot.ToString() + TEXT(" (ProtoBoy > Connect Task Board)")));
			return nullptr;
		}
		return Tasks;
	}

	// -----------------------------------------------------------------------------------------------------------------
	// Formatting

	FString StatusLabel(const FProtoBoyTask& Task)
	{
		const FString Status = ProtoBoyTasks::StatusToString(Task.Status);
		return Task.StatusName.IsEmpty() ? Status : FString::Printf(TEXT("%s, list \"%s\""), *Status, *Task.StatusName);
	}

	FString FormatTaskLine(const FProtoBoyTask& Task)
	{
		FString Line = FString::Printf(TEXT("- [%s] %s (%s)"), *Task.Id, *Task.Title, *StatusLabel(Task));
		if (!Task.Labels.IsEmpty())
		{
			Line += FString::Printf(TEXT(" labels: %s"), *FString::Join(Task.Labels, TEXT(", ")));
		}
		if (!Task.DesignDoc.IsEmpty())
		{
			Line += FString::Printf(TEXT(" design doc: %s"), *Task.DesignDoc);
		}
		return Line;
	}

	FString FormatTask(const FProtoBoyTask& Task)
	{
		FString Out = FString::Printf(TEXT("# %s\n\n- **Id:** %s\n- **Status:** %s\n- **Url:** %s\n"),
			*Task.Title, *Task.Id, *StatusLabel(Task), *Task.Url);
		if (!Task.Labels.IsEmpty())
		{
			Out += FString::Printf(TEXT("- **Labels:** %s\n"), *FString::Join(Task.Labels, TEXT(", ")));
		}
		if (!Task.DueUtc.IsEmpty())
		{
			Out += FString::Printf(TEXT("- **Due:** %s\n"), *Task.DueUtc);
		}
		Out += FString::Printf(TEXT("- **Design doc:** %s\n"), Task.DesignDoc.IsEmpty()
			? TEXT("none linked (link one with a \"Design doc: <path>\" line, or ask the user which doc applies)")
			: *Task.DesignDoc);
		Out += FString::Printf(TEXT("\n## Description\n\n%s\n"), Task.Description.IsEmpty() ? TEXT("_No description._") : *Task.Description);
		if (!Task.Comments.IsEmpty())
		{
			Out += TEXT("\n## Comments (newest first)\n\n");
			for (const FProtoBoyTaskComment& Comment : Task.Comments)
			{
				Out += FString::Printf(TEXT("- **%s** (%s): %s\n"), *Comment.Author, *Comment.DateUtc, *Comment.Text.Replace(TEXT("\n"), TEXT(" ")));
			}
		}
		return Out;
	}

	FString OrNotSet(const FString& Value)
	{
		return Value.IsEmpty() ? FString(TEXT("_Not written yet._")) : Value;
	}

	/** Lowercase words of 3+ letters, minus filler words, for matching feature descriptions to docs. */
	TArray<FString> Keywords(const FString& Text)
	{
		static const TSet<FString> Stop = { TEXT("the"), TEXT("and"), TEXT("for"), TEXT("with"), TEXT("that"), TEXT("this"),
			TEXT("from"), TEXT("into"), TEXT("when"), TEXT("should"), TEXT("will"), TEXT("can"), TEXT("add"), TEXT("make"),
			TEXT("new"), TEXT("player"), TEXT("game"), TEXT("feature"), TEXT("system") };

		TArray<FString> Words;
		FString Current;
		auto Flush = [&]()
		{
			if (Current.Len() >= 3 && !Stop.Contains(Current))
			{
				Words.AddUnique(Current);
			}
			Current.Reset();
		};
		for (const TCHAR C : Text)
		{
			if (FChar::IsAlnum(C)) { Current.AppendChar(FChar::ToLower(C)); }
			else { Flush(); }
		}
		Flush();
		return Words;
	}

	/** "- [design] Design/Combat/Combat.md: Combat (Tech doc: Design/Combat/Combat.tech.md)", nested under its parent. */
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

	/** True if Path is the Vision or Overview, which humans own. */
	bool IsHumanOwnedDoc(const FProtoBoyDocMap& Map, const FString& Path)
	{
		const FProtoBoyDocNode* Node = Map.FindByPath(Path);
		return Node && (Node->Layer == EProtoBoyDocLayer::Vision || Node->Layer == EProtoBoyDocLayer::Overview);
	}

	// -----------------------------------------------------------------------------------------------------------------
	// Context and placement

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
		const FProtoBoyVision Vision = Map.ReadVision();

		FString Out = TEXT("# Project context (ProtoBoy)\n\n");
		Out += FString::Printf(TEXT("- **Docs:** %s\n"), *DocsProvider.GetDisplayName().ToString());
		Out += TEXT("- **Doc layers:** Vision (why) > Overview (what, and an index) > Design (each feature, player side) > "
			"Tech (how a Design doc is built) / Systems (shared tech) / Content (what exists) / Direction (look, sound, writing) / "
			"Decision (ADRs). Read down the layers before building.\n");

		if (!Vision.bExists)
		{
			Out += TEXT("\n**There is no Vision doc yet.** Without it you can't tell whether new work serves the game. "
				"Ask the user to create the Vision and Overview (ProtoBoy dashboard > Architecture > Doc Map), or offer to draft "
				"them for review.\n");
		}
		else
		{
			Out += FString::Printf(TEXT("- **Vision:** %s (read it in full with docs_read)\n"), *Vision.Path);
			Out += FString::Printf(TEXT("\n## Pitch\n\n%s\n"), *OrNotSet(Vision.Pitch));
			Out += TEXT("\n## Design pillars\n\nEvery feature should serve at least one. If a change serves none, say so before building it.\n\n");
			if (Vision.Pillars.IsEmpty())
			{
				Out += TEXT("_None written yet._\n");
			}
			for (const FProtoBoyDesignPillar& Pillar : Vision.Pillars)
			{
				Out += FString::Printf(TEXT("### %s\n\n%s\n\n"), *Pillar.Name, *OrNotSet(Pillar.Text));
			}
			Out += FString::Printf(TEXT("\n## Non-goals\n\n%s\n"), *OrNotSet(Vision.NonGoals));
		}
		if (const FProtoBoyDocNode* Overview = Map.GetOverview())
		{
			Out += FString::Printf(TEXT("\n## Overview (%s)\n\n%s\n"), *Overview->Path, *OrNotSet(Map.ReadOverviewSummary()));
		}

		Out += FString::Printf(TEXT("\n## Asset sourcing\n\n%s"), *ProtoBoyAssetSourcing::DescribePolicyForAgents());

		Out += TEXT("\n## Doc map\n\n");
		if (const FProtoBoyDocNode* Root = Map.GetVision())
		{
			AppendDocTree(Map, *Root, 0, Out);
		}
		else
		{
			Out += TEXT("_No map yet (no Vision doc)._\n");
		}
		if (!Map.GetUnmappedDocs().IsEmpty())
		{
			Out += FString::Printf(TEXT("\n%d other docs aren't on the map (no layer); find them with docs_list or docs_search.\n"), Map.GetUnmappedDocs().Num());
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
							Out += FormatTaskLine(Task) + TEXT("\n");
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

	void LocateFeature(const FProtoBoyMCPToolCall& Call, FProtoBoyMCPToolDone Done)
	{
		UProtoBoySubsystem* Subsystem = GetSubsystemOrFail(Done);
		if (!Subsystem || !Require(Call, Done, { TEXT("description") }))
		{
			return;
		}

		FProtoBoyDocMap& Map = Subsystem->GetDocMap();
		Map.Refresh();
		const IProtoBoyDocsProvider& Provider = Subsystem->GetDocsProvider();

		// Features are described in Design docs and shared tech in Systems docs: those are where new work gets placed.
		struct FScored { const FProtoBoyDocNode* Doc; FString Text; int32 Score; };
		TArray<FScored> Scored;
		const TArray<FString> Words = Keywords(GetString(Call, TEXT("description")));
		for (const FProtoBoyDocNode& Doc : Map.GetNodes())
		{
			if (Doc.Layer != EProtoBoyDocLayer::Design && Doc.Layer != EProtoBoyDocLayer::Systems)
			{
				continue;
			}
			FString Text;
			FText Ignored;
			Provider.ReadDoc(Doc.Path, Text, Ignored);
			const FString Title = Doc.Title.ToLower();
			const FString Lower = Text.ToLower();
			int32 Score = 0;
			for (const FString& Word : Words)
			{
				Score += Title.Contains(Word) ? 5 : 0;
				Score += Lower.Contains(Word) ? 2 : 0;
			}
			Scored.Add({ &Doc, MoveTemp(Text), Score });
		}

		if (Scored.IsEmpty())
		{
			Done(FProtoBoyMCPToolResult::Error(TEXT("There are no Design or Systems docs on the doc map yet, so ProtoBoy can't place this. "
				"Ask the user where it belongs, or propose a new Design doc for them to add from the Doc Map.")));
			return;
		}
		Scored.Sort([](const FScored& A, const FScored& B) { return A.Score > B.Score; });

		FString Out;
		if (Scored[0].Score == 0)
		{
			Out = TEXT("No Design or Systems doc clearly matches this. Either it extends one of the docs below (pick the closest "
				"and say why), or it's a new feature or system: propose a new doc to the user, check it against the Vision's "
				"design pillars, and record the decision with adr_create before writing code.\n\n## Design and Systems docs\n\n");
			for (const FScored& Entry : Scored)
			{
				Out += FString::Printf(TEXT("- [%s] %s: %s\n"), *ProtoBoyDocLayers::ToKey(Entry.Doc->Layer), *Entry.Doc->Path, *Entry.Doc->Title);
			}
		}
		else
		{
			Out = TEXT("Best matches by keyword (check them against the docs themselves; keyword matching is a hint, not a decision):\n");
			for (int32 i = 0; i < Scored.Num() && i < 3 && Scored[i].Score > 0; ++i)
			{
				const FProtoBoyDocNode& Doc = *Scored[i].Doc;
				Out += FString::Printf(TEXT("\n## %s (%s, %s)\n\n"), *Doc.Title, *ProtoBoyDocLayers::GetDisplayName(Doc.Layer).ToString(), *Doc.Path);
				if (const FProtoBoyDocNode* Tech = Doc.Layer == EProtoBoyDocLayer::Design ? Map.FindTechFor(Doc.Id) : nullptr)
				{
					Out += FString::Printf(TEXT("How it's built: %s (read it before changing code).\n\n"), *Tech->Path);
				}
				else if (Doc.Layer == EProtoBoyDocLayer::Design)
				{
					Out += TEXT("It has no Tech doc yet: how it's built isn't written down.\n\n");
				}
				Out += Scored[i].Text.Left(4000) + TEXT("\n");
			}
		}
		Done(FProtoBoyMCPToolResult::Ok(Out));
	}

	// -----------------------------------------------------------------------------------------------------------------
	// Docs

	void DocsList(const FProtoBoyMCPToolCall& Call, FProtoBoyMCPToolDone Done)
	{
		UProtoBoySubsystem* Subsystem = GetSubsystemOrFail(Done);
		if (!Subsystem)
		{
			return;
		}

		FString Folder = GetString(Call, TEXT("folder")).Replace(TEXT("\\"), TEXT("/"));
		if (!Folder.IsEmpty() && !Folder.EndsWith(TEXT("/")))
		{
			Folder += TEXT("/");
		}

		const IProtoBoyDocsProvider& Provider = Subsystem->GetDocsProvider();
		FString Out = FString::Printf(TEXT("Docs in %s%s:\n\n"), *Provider.GetDisplayName().ToString(),
			Folder.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" under %s"), *Folder));
		int32 Count = 0;
		for (const FProtoBoyDocInfo& Doc : Provider.ListDocs())
		{
			if (Folder.IsEmpty() || Doc.Path.StartsWith(Folder, ESearchCase::IgnoreCase))
			{
				Out += FString::Printf(TEXT("- %s: %s (modified %s)\n"), *Doc.Path, *Doc.Title, *Doc.ModifiedUtc.ToString(TEXT("%Y-%m-%d")));
				++Count;
			}
		}
		if (Count == 0)
		{
			Out += TEXT("_No docs._\n");
		}
		Done(FProtoBoyMCPToolResult::Ok(Out));
	}

	void DocsSearch(const FProtoBoyMCPToolCall& Call, FProtoBoyMCPToolDone Done)
	{
		UProtoBoySubsystem* Subsystem = GetSubsystemOrFail(Done);
		if (!Subsystem || !Require(Call, Done, { TEXT("query") }))
		{
			return;
		}

		const TArray<FProtoBoyDocSearchHit> Hits = Subsystem->GetDocGenerator().Search(GetString(Call, TEXT("query")), GetInt(Call, TEXT("limit"), 10, 1, 50));
		if (Hits.IsEmpty())
		{
			Done(FProtoBoyMCPToolResult::Ok(TEXT("No docs match. Try other words, or docs_list to browse.")));
			return;
		}

		FString Out;
		for (const FProtoBoyDocSearchHit& Hit : Hits)
		{
			Out += FString::Printf(TEXT("- **%s** (%s): ...%s...\n"), *Hit.Path, *Hit.Title, *Hit.Snippet);
		}
		Done(FProtoBoyMCPToolResult::Ok(Out));
	}

	void DocsRead(const FProtoBoyMCPToolCall& Call, FProtoBoyMCPToolDone Done)
	{
		UProtoBoySubsystem* Subsystem = GetSubsystemOrFail(Done);
		if (!Subsystem || !Require(Call, Done, { TEXT("path") }))
		{
			return;
		}

		const FString Path = GetString(Call, TEXT("path"));
		FString Text;
		FText Error;
		if (!Subsystem->GetDocsProvider().ReadDoc(Path, Text, Error))
		{
			Done(FProtoBoyMCPToolResult::Error(Error));
			return;
		}

		const FString Section = GetString(Call, TEXT("section"));
		if (!Section.IsEmpty())
		{
			const FString Body = ProtoBoyMarkdown::GetSectionBody(Text, Section);
			if (Body.IsEmpty() && !ProtoBoyMarkdown::FindSection(ProtoBoyMarkdown::ParseSections(Text), Section))
			{
				Done(FProtoBoyMCPToolResult::Error(FString::Printf(TEXT("\"%s\" has no section \"%s\"."), *Path, *Section)));
				return;
			}
			Text = FString::Printf(TEXT("## %s\n\n%s\n"), *Section, *Body);
		}

		constexpr int32 MaxChars = 100000;
		if (Text.Len() > MaxChars)
		{
			Text = Text.Left(MaxChars) + TEXT("\n\n[Truncated. Read one section at a time with the \"section\" argument.]");
		}
		Done(FProtoBoyMCPToolResult::Ok(Text));
	}

	void DocsCreate(const FProtoBoyMCPToolCall& Call, FProtoBoyMCPToolDone Done)
	{
		UProtoBoySubsystem* Subsystem = GetSubsystemOrFail(Done);
		if (!Subsystem || !Require(Call, Done, { TEXT("path"), TEXT("content") }))
		{
			return;
		}

		const FString Path = GetString(Call, TEXT("path"));
		FText Error;
		if (!Subsystem->GetDocGenerator().CreateDoc(Path, GetString(Call, TEXT("content")) + TEXT("\n"), Error))
		{
			Done(FProtoBoyMCPToolResult::Error(Error));
			return;
		}
		Done(FProtoBoyMCPToolResult::Ok(FString::Printf(TEXT("Created %s."), *Path)));
	}

	void DocsUpdateSection(const FProtoBoyMCPToolCall& Call, FProtoBoyMCPToolDone Done)
	{
		UProtoBoySubsystem* Subsystem = GetSubsystemOrFail(Done);
		if (!Subsystem || !Require(Call, Done, { TEXT("path"), TEXT("heading"), TEXT("content") }))
		{
			return;
		}

		const FString Path = GetString(Call, TEXT("path"));
		const FString Heading = GetString(Call, TEXT("heading"));
		const FString Content = GetString(Call, TEXT("content"));
		const FString Mode = GetString(Call, TEXT("mode")).IsEmpty() ? FString(TEXT("append")) : GetString(Call, TEXT("mode"));

		FString NormalizedPath;
		FText PathError;
		const bool bHumanOwned = FProtoBoyDocGenerator::NormalizeDocPath(Path, NormalizedPath, PathError)
			&& IsHumanOwnedDoc(Subsystem->GetDocMap(), NormalizedPath);
		if (bHumanOwned && Mode != TEXT("append"))
		{
			Done(FProtoBoyMCPToolResult::Error(FString::Printf(TEXT("%s is the Vision or Overview, which humans own: agents may only append to it, and only when the user asks."), *NormalizedPath)));
			return;
		}

		FText Error;
		bool bOk = false;
		if (Mode == TEXT("append"))
		{
			bOk = Subsystem->GetDocGenerator().AppendToSection(Path, Heading, Content, Error);
		}
		else if (Mode == TEXT("managed"))
		{
			if (!Require(Call, Done, { TEXT("block_id") }))
			{
				return;
			}
			bOk = Subsystem->GetDocGenerator().SetManagedBlock(Path, Heading, GetString(Call, TEXT("block_id")), Content, Error);
		}
		else
		{
			Done(FProtoBoyMCPToolResult::Error(TEXT("mode must be \"append\" or \"managed\".")));
			return;
		}

		if (!bOk)
		{
			Done(FProtoBoyMCPToolResult::Error(Error));
			return;
		}
		Done(FProtoBoyMCPToolResult::Ok(FString::Printf(TEXT("Updated section \"%s\" in %s (%s)."), *Heading, *Path, *Mode)));
	}

	void AdrCreate(const FProtoBoyMCPToolCall& Call, FProtoBoyMCPToolDone Done)
	{
		UProtoBoySubsystem* Subsystem = GetSubsystemOrFail(Done);
		if (!Subsystem || !Require(Call, Done, { TEXT("title"), TEXT("decision") }))
		{
			return;
		}

		FString Path;
		FText Error;
		if (!Subsystem->GetDocGenerator().CreateADR(GetString(Call, TEXT("title")), GetString(Call, TEXT("context")),
			GetString(Call, TEXT("decision")), GetString(Call, TEXT("consequences")), GetString(Call, TEXT("status")),
			FString::Printf(TEXT("%s via ProtoBoy"), *Call.ClientName), Path, Error))
		{
			Done(FProtoBoyMCPToolResult::Error(Error));
			return;
		}
		Subsystem->GetDocMap().Refresh();
		Done(FProtoBoyMCPToolResult::Ok(FString::Printf(TEXT("Created %s. If it changes a Design, Tech or Systems doc, propose the matching edit to the user."), *Path)));
	}

	// -----------------------------------------------------------------------------------------------------------------
	// Tasks

	void TasksList(const FProtoBoyMCPToolCall& Call, FProtoBoyMCPToolDone Done)
	{
		IProtoBoyTaskProvider* Tasks = GetTasksOrFail(Done);
		if (!Tasks)
		{
			return;
		}

		const FString StatusArg = GetString(Call, TEXT("status"));
		TOptional<EProtoBoyTaskStatus> Filter;
		if (!StatusArg.IsEmpty() && StatusArg != TEXT("all"))
		{
			Filter = ProtoBoyTasks::StatusFromString(StatusArg);
			if (!Filter.IsSet())
			{
				Done(FProtoBoyMCPToolResult::Error(TEXT("status must be todo, in_progress, done or all.")));
				return;
			}
		}

		const FString BoardName = Tasks->GetDisplayName().ToString();
		Tasks->ListTasks(Filter, [BoardName, Done = MoveTemp(Done)](const TProtoBoyTaskResult<TArray<FProtoBoyTask>>& Result)
		{
			if (!Result.bOk)
			{
				Done(FProtoBoyMCPToolResult::Error(Result.Error));
				return;
			}
			FString Out = FString::Printf(TEXT("%d open tasks on %s:\n\n"), Result.Value.Num(), *BoardName);
			for (const FProtoBoyTask& Task : Result.Value)
			{
				Out += FormatTaskLine(Task) + TEXT("\n");
			}
			Done(FProtoBoyMCPToolResult::Ok(Out));
		});
	}

	void TaskGet(const FProtoBoyMCPToolCall& Call, FProtoBoyMCPToolDone Done)
	{
		IProtoBoyTaskProvider* Tasks = GetTasksOrFail(Done);
		if (!Tasks || !Require(Call, Done, { TEXT("id") }))
		{
			return;
		}

		Tasks->GetTask(GetString(Call, TEXT("id")), [Done = MoveTemp(Done)](const TProtoBoyTaskResult<FProtoBoyTask>& Result)
		{
			Done(Result.bOk ? FProtoBoyMCPToolResult::Ok(FormatTask(Result.Value)) : FProtoBoyMCPToolResult::Error(Result.Error));
		});
	}

	void TaskCreate(const FProtoBoyMCPToolCall& Call, FProtoBoyMCPToolDone Done)
	{
		IProtoBoyTaskProvider* Tasks = GetTasksOrFail(Done);
		if (!Tasks || !Require(Call, Done, { TEXT("title") }))
		{
			return;
		}

		const FString StatusArg = GetString(Call, TEXT("status"));
		const TOptional<EProtoBoyTaskStatus> Status = StatusArg.IsEmpty() ? TOptional<EProtoBoyTaskStatus>(EProtoBoyTaskStatus::ToDo) : ProtoBoyTasks::StatusFromString(StatusArg);
		if (!Status.IsSet())
		{
			Done(FProtoBoyMCPToolResult::Error(TEXT("status must be todo, in_progress or done.")));
			return;
		}

		FString Description = GetString(Call, TEXT("description"));
		const FString DesignDoc = GetString(Call, TEXT("design_doc"));
		if (!DesignDoc.IsEmpty())
		{
			Description += FString::Printf(TEXT("\n\n%s"), *ProtoBoyTasks::MakeDesignDocLine(DesignDoc));
		}
		Description += FString::Printf(TEXT("\n\n_Created by %s via ProtoBoy._"), *Call.ClientName);

		Tasks->CreateTask(GetString(Call, TEXT("title")), Description.TrimStart(), Status.GetValue(),
			[Done = MoveTemp(Done)](const TProtoBoyTaskResult<FProtoBoyTask>& Result)
			{
				Done(Result.bOk
					? FProtoBoyMCPToolResult::Ok(FString::Printf(TEXT("Created task [%s] %s (%s)."), *Result.Value.Id, *Result.Value.Title, *Result.Value.Url))
					: FProtoBoyMCPToolResult::Error(Result.Error));
			});
	}

	void TaskUpdateStatus(const FProtoBoyMCPToolCall& Call, FProtoBoyMCPToolDone Done)
	{
		IProtoBoyTaskProvider* Tasks = GetTasksOrFail(Done);
		if (!Tasks || !Require(Call, Done, { TEXT("id"), TEXT("status") }))
		{
			return;
		}

		const TOptional<EProtoBoyTaskStatus> Status = ProtoBoyTasks::StatusFromString(GetString(Call, TEXT("status")));
		if (!Status.IsSet())
		{
			Done(FProtoBoyMCPToolResult::Error(TEXT("status must be todo, in_progress or done.")));
			return;
		}

		Tasks->SetTaskStatus(GetString(Call, TEXT("id")), Status.GetValue(), [Done = MoveTemp(Done)](const TProtoBoyTaskResult<FProtoBoyTask>& Result)
		{
			Done(Result.bOk
				? FProtoBoyMCPToolResult::Ok(FString::Printf(TEXT("[%s] %s is now %s."), *Result.Value.Id, *Result.Value.Title, *ProtoBoyTasks::StatusToString(Result.Value.Status)))
				: FProtoBoyMCPToolResult::Error(Result.Error));
		});
	}

	void TaskComment(const FProtoBoyMCPToolCall& Call, FProtoBoyMCPToolDone Done)
	{
		IProtoBoyTaskProvider* Tasks = GetTasksOrFail(Done);
		if (!Tasks || !Require(Call, Done, { TEXT("id"), TEXT("text") }))
		{
			return;
		}

		const FString Text = FString::Printf(TEXT("%s\n\n_%s via ProtoBoy_"), *GetString(Call, TEXT("text")), *Call.ClientName);
		Tasks->AddComment(GetString(Call, TEXT("id")), Text, [Done = MoveTemp(Done)](const TProtoBoyTaskResult<bool>& Result)
		{
			Done(Result.bOk ? FProtoBoyMCPToolResult::Ok(TEXT("Comment added.")) : FProtoBoyMCPToolResult::Error(Result.Error));
		});
	}

	// -----------------------------------------------------------------------------------------------------------------
	// Registration

	void RegisterTools(TArray<FProtoBoyMCPTool>& OutTools)
	{
		const TArray<FString> StatusValues = { TEXT("todo"), TEXT("in_progress"), TEXT("done") };
		const TArray<FString> ListStatusValues = { TEXT("todo"), TEXT("in_progress"), TEXT("done"), TEXT("all") };

		auto Add = [&OutTools](const TCHAR* Name, const FText& Title, const FText& Description, bool bWrites,
			const TArray<FParam>& Params, FProtoBoyMCPToolHandler Handler)
		{
			OutTools.Add({ FName(Name), Title, Description, bWrites, MakeSchema(Params), MoveTemp(Handler) });
		};

		OutTools.Reset();

		// Read
		Add(TEXT("get_project_context"), LOCTEXT("ContextTitle", "Get project context"),
			LOCTEXT("ContextDesc", "Start here. Returns the Vision (pitch, design pillars, non-goals), the Overview's summary, the doc map (every doc by layer) and open tasks."),
			false, {}, &GetProjectContext);

		Add(TEXT("locate_feature"), LOCTEXT("LocateTitle", "Locate feature"),
			LOCTEXT("LocateDesc", "Finds the Design or Systems doc a feature or change belongs to, and its Tech doc. Use before writing code."),
			false, { { TEXT("description"), TEXT("string"), TEXT("What you're about to build or change."), true } }, &LocateFeature);

		Add(TEXT("docs_list"), LOCTEXT("ListTitle", "List docs"),
			LOCTEXT("ListDesc", "Lists the project's docs (Vision, Overview, Design, Tech, ADRs, ...)."),
			false, { { TEXT("folder"), TEXT("string"), TEXT("Only list docs under this folder, e.g. \"Design\" or \"ADR\".") } }, &DocsList);

		Add(TEXT("docs_search"), LOCTEXT("SearchTitle", "Search docs"),
			LOCTEXT("SearchDesc", "Keyword search over the project's docs."),
			false, {
				{ TEXT("query"), TEXT("string"), TEXT("Words to look for."), true },
				{ TEXT("limit"), TEXT("integer"), TEXT("Maximum results (default 10).") },
			}, &DocsSearch);

		Add(TEXT("docs_read"), LOCTEXT("ReadTitle", "Read doc"),
			LOCTEXT("ReadDesc", "Reads a doc, or one section of it."),
			false, {
				{ TEXT("path"), TEXT("string"), TEXT("Doc path relative to the docs root, e.g. \"Design/Combat.md\"."), true },
				{ TEXT("section"), TEXT("string"), TEXT("Only return the section under this heading.") },
			}, &DocsRead);

		Add(TEXT("tasks_list"), LOCTEXT("TasksTitle", "List tasks"),
			LOCTEXT("TasksDesc", "Lists open tasks on the project's task board."),
			false, { { TEXT("status"), TEXT("string"), TEXT("Filter by status (default all)."), false, ListStatusValues } }, &TasksList);

		Add(TEXT("task_get"), LOCTEXT("TaskGetTitle", "Get task"),
			LOCTEXT("TaskGetDesc", "Gets a task's description, status, linked design doc and recent comments."),
			false, { { TEXT("id"), TEXT("string"), TEXT("Task id from tasks_list."), true } }, &TaskGet);

		// Write
		Add(TEXT("docs_create"), LOCTEXT("CreateTitle", "Create doc"),
			LOCTEXT("CreateDesc", "Creates a new Markdown doc (fails if it exists). Put feature designs under Design/."),
			true, {
				{ TEXT("path"), TEXT("string"), TEXT("New doc path, e.g. \"Design/Inventory.md\"."), true },
				{ TEXT("content"), TEXT("string"), TEXT("Markdown content, starting with a \"# Title\" heading."), true },
			}, &DocsCreate);

		Add(TEXT("docs_update_section"), LOCTEXT("UpdateTitle", "Update doc section"),
			LOCTEXT("UpdateDesc", "Adds to a doc without rewriting human text: \"append\" adds to the end of a section (created if missing); \"managed\" replaces a ProtoBoy-managed block in that section."),
			true, {
				{ TEXT("path"), TEXT("string"), TEXT("Doc path; created if it doesn't exist."), true },
				{ TEXT("heading"), TEXT("string"), TEXT("Section heading, without #."), true },
				{ TEXT("content"), TEXT("string"), TEXT("Markdown to add."), true },
				{ TEXT("mode"), TEXT("string"), TEXT("append (default) or managed."), false, { TEXT("append"), TEXT("managed") } },
				{ TEXT("block_id"), TEXT("string"), TEXT("For managed mode: block id (lowercase letters, digits, -).") },
			}, &DocsUpdateSection);

		Add(TEXT("adr_create"), LOCTEXT("AdrTitle", "Create ADR"),
			LOCTEXT("AdrDesc", "Records an Architecture Decision Record. Required before adding a new feature, system or module the docs don't cover."),
			true, {
				{ TEXT("title"), TEXT("string"), TEXT("Short decision title."), true },
				{ TEXT("decision"), TEXT("string"), TEXT("What was decided."), true },
				{ TEXT("context"), TEXT("string"), TEXT("Why the decision came up.") },
				{ TEXT("consequences"), TEXT("string"), TEXT("What follows, including what gets harder.") },
				{ TEXT("status"), TEXT("string"), TEXT("Proposed (default), Accepted, Superseded."), false, { TEXT("Proposed"), TEXT("Accepted"), TEXT("Superseded") } },
			}, &AdrCreate);

		Add(TEXT("task_create"), LOCTEXT("TaskCreateTitle", "Create task"),
			LOCTEXT("TaskCreateDesc", "Creates a task on the board. Link the design doc it implements."),
			true, {
				{ TEXT("title"), TEXT("string"), TEXT("Task title."), true },
				{ TEXT("description"), TEXT("string"), TEXT("What to do and the acceptance criteria.") },
				{ TEXT("status"), TEXT("string"), TEXT("Initial status (default todo)."), false, StatusValues },
				{ TEXT("design_doc"), TEXT("string"), TEXT("Design doc path this task implements, e.g. \"Design/Inventory.md\".") },
			}, &TaskCreate);

		Add(TEXT("task_update_status"), LOCTEXT("TaskStatusTitle", "Update task status"),
			LOCTEXT("TaskStatusDesc", "Moves a task to todo, in_progress or done."),
			true, {
				{ TEXT("id"), TEXT("string"), TEXT("Task id."), true },
				{ TEXT("status"), TEXT("string"), TEXT("New status."), true, StatusValues },
			}, &TaskUpdateStatus);

		Add(TEXT("task_comment"), LOCTEXT("TaskCommentTitle", "Comment on task"),
			LOCTEXT("TaskCommentDesc", "Adds a progress comment to a task (what changed, follow-ups)."),
			true, {
				{ TEXT("id"), TEXT("string"), TEXT("Task id."), true },
				{ TEXT("text"), TEXT("string"), TEXT("Comment text (Markdown)."), true },
			}, &TaskComment);

		// Feature areas register their own tools.
		ProtoBoySourcingMCP::RegisterTools(OutTools);
	}
}

#undef LOCTEXT_NAMESPACE
