#include "Tasks/Generation/PBTaskGeneration.h"

#include "Agents/PBAgentPromptContext.h"
#include "Docs/PBDocMap.h"
#include "Dom/JsonObject.h"
#include "MCP/PBMCPClientConfig.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "PBSubsystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Tasks/PBTaskBoard.h"
#include "Tasks/PBTaskProvider.h"

#define LOCTEXT_NAMESPACE "ProtoBoyTaskGeneration"

namespace ProtoBoyTaskGeneration
{
	/** Per doc, so a big doc can't crowd out the others. */
	constexpr int32 MaxDocChars = 12000;
	/** More than this is ignored: the agent is asked for far fewer. */
	constexpr int32 MaxTasks = 60;

	/** The category's doc and every doc below it on the map, depth first. */
	FString DescribeCategory(const FProtoBoyDocMap& Map, const FProtoBoyDocNode& Root, const TArray<FString>& ExistingTasks)
	{
		FString Out = FString::Printf(TEXT("## Category: %s\n\n"), *Root.Title);
		Out += TEXT("Tasks already in its To Do list (don't repeat these):\n\n");
		for (const FString& Task : ExistingTasks)
		{
			Out += FString::Printf(TEXT("- %s\n"), *Task);
		}
		if (ExistingTasks.IsEmpty())
		{
			Out += TEXT("_None._\n");
		}

		TArray<TPair<const FProtoBoyDocNode*, int32>> Stack = { { &Root, 0 } };
		while (!Stack.IsEmpty())
		{
			const TPair<const FProtoBoyDocNode*, int32> Entry = Stack.Pop();
			const FProtoBoyDocNode& Doc = *Entry.Key;
			Out += FString::Printf(TEXT("\n### %s doc \"%s\" (%s)%s\n\n"), *ProtoBoyDocLayers::GetDisplayName(Doc.Layer).ToString(), *Doc.Title, *Doc.Path,
				Entry.Value == 0 ? TEXT("") : TEXT(", below the category's doc"));
			Out += ProtoBoyAgentPrompt::ReadDoc(Doc, MaxDocChars);
			const TArray<const FProtoBoyDocNode*> Children = Map.GetChildren(Doc.Id);
			for (int32 Index = Children.Num() - 1; Index >= 0; --Index)
			{
				Stack.Push({ Children[Index], Entry.Value + 1 });
			}
		}
		return Out + TEXT("\n");
	}

	FString BuildPrompt(const FString& Categories, const TArray<FString>& Names, const FString& ExtraContext)
	{
		const FString Context = ExtraContext.TrimStartAndEnd().IsEmpty()
			? FString(TEXT("_None._"))
			: ExtraContext.TrimStartAndEnd();
		return FString::Printf(TEXT(R"MD(# Draft tasks for the Unreal Engine project "%s"

You are drafting task ideas for the team's task board. Each task category below is a doc on the project's doc map,
with the docs below it. Suggest the work still needed to build what those docs describe.

## Rules

- Read only. Don't create, edit or delete any file, and don't run commands.
- You may read the project (Source/, Config/, the .uproject, project plugins except Plugins/ProtoBoy) to see what's
  already built, so you suggest what's missing rather than what exists.
- As you work, print one short line per step that starts with "PROGRESS: ", e.g. "PROGRESS: Checking the inventory
  code". Keep them short; they're shown to the user as a progress log.
- Each task is small enough for one work session, says what to do and how to tell it's done (acceptance criteria),
  and serves what its category's docs describe. Don't invent features the docs don't mention.
- Don't repeat tasks already in a category's To Do list.
- Usually 3-8 tasks per category; fewer if the docs are thin.

## Extra context from the user

%s

%s## Your answer

When you're done, reply with only this JSON object inside a ```json fence, and nothing after it:

```json
{
  "tasks": [
    {
      "category": "%s",
      "title": "Short imperative title",
      "description": "What to do, in a few sentences.\n\nAcceptance criteria:\n- ..."
    }
  ]
}
```

"category" is exactly one of: %s.
)MD"), FApp::GetProjectName(), *Context, *Categories, Names.IsEmpty() ? TEXT("") : *Names[0],
			*FString::JoinBy(Names, TEXT(", "), [](const FString& Name) { return FString::Printf(TEXT("\"%s\""), *Name); }));
	}
}

FProtoBoyTaskGenerationRun::~FProtoBoyTaskGenerationRun()
{
	Agent.Stop();
}

FString FProtoBoyTaskGenerationRun::GetWorkDir()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ProtoBoy") / TEXT("TaskGeneration"));
}

bool FProtoBoyTaskGenerationRun::Start(EProtoBoyMCPClient Client, const TArray<FString>& CategoryIds, const FString& ExtraContext, FText& OutError)
{
	if (IsRunning() || bFinished)
	{
		return false;
	}
	UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	IProtoBoyTaskProvider* Tasks = Subsystem ? Subsystem->GetTaskProvider() : nullptr;
	if (!Tasks || !Tasks->IsReady(OutError))
	{
		if (!Tasks)
		{
			OutError = LOCTEXT("NoBoard", "No task board is connected.");
		}
		return false;
	}

	const FProtoBoyDocMap& Map = Subsystem->GetDocMap();
	FString Described;
	TArray<FString> Names;
	const FProtoBoyTaskBoard& Board = Subsystem->GetTaskBoard();
	for (const FString& DocId : CategoryIds)
	{
		const FProtoBoyDocNode* Doc = Map.Find(DocId);
		if (!Doc)
		{
			continue;
		}
		// What's already in its To Do list (from the last Sync), so the agent doesn't repeat it.
		TArray<FString> Existing;
		if (const FProtoBoyTrelloCategorySync::FCategoryList* List = Board.FindList(DocId))
		{
			for (const FProtoBoyTask& Task : List->Tasks)
			{
				Existing.Add(Task.Title);
			}
		}
		Described += ProtoBoyTaskGeneration::DescribeCategory(Map, *Doc, Existing);
		Names.Add(Doc->Title);
		Picked.Add({ Doc->Id, Doc->Title, Doc->Path });
	}
	if (Names.IsEmpty())
	{
		OutError = LOCTEXT("NoCategories", "Pick at least one task category.");
		return false;
	}

	Agent.AddLog(FText::Format(LOCTEXT("LogReading", "Reading the docs of {0} {0}|plural(one=category,other=categories) for the agent..."), FText::AsNumber(Names.Num())).ToString());
	FProtoBoyAgentRun::FParams Params;
	Params.Client = Client;
	Params.Prompt = ProtoBoyTaskGeneration::BuildPrompt(Described, Names, ExtraContext);
	Params.WorkDir = GetWorkDir();
	Params.StartedMessage = FText::Format(LOCTEXT("LogStarted", "Started {0}. It reads the docs and the project, then drafts tasks; this can take a few minutes."),
		FProtoBoyMCPClientConfig::GetClientName(Client));
	Params.AnswerMessage = LOCTEXT("LogWriting", "The agent is writing the tasks...");
	return Agent.Start(Params, OutError);
}

void FProtoBoyTaskGenerationRun::Tick()
{
	if (Agent.Tick())
	{
		Complete();
	}
}

void FProtoBoyTaskGenerationRun::Complete()
{
	bFinished = true;
	const FString Json = Agent.ExtractJson();
	TSharedPtr<FJsonObject> Root;
	const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
	if (Json.IsEmpty())
	{
		Agent.AddLog(Agent.DescribeNoAnswer().ToString());
		return;
	}
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid() || !Root->TryGetArrayField(TEXT("tasks"), Array))
	{
		Agent.AddLog(FText::Format(LOCTEXT("LogBadJson", "The agent's answer isn't the expected JSON. Its output is in {0}."), FText::FromString(Agent.GetOutputLogPath())).ToString());
		return;
	}

	// Each idea in a picked category (as the doc map names it), else Uncategorized.
	for (const TSharedPtr<FJsonValue>& Value : *Array)
	{
		const TSharedPtr<FJsonObject>* Task = nullptr;
		if (!Value->TryGetObject(Task) || Ideas.Num() >= ProtoBoyTaskGeneration::MaxTasks)
		{
			continue;
		}
		FString Category, Title, Description;
		(*Task)->TryGetStringField(TEXT("category"), Category);
		(*Task)->TryGetStringField(TEXT("title"), Title);
		(*Task)->TryGetStringField(TEXT("description"), Description);
		Title = Title.TrimStartAndEnd();
		if (Title.IsEmpty())
		{
			continue;
		}
		const FPicked* Doc = Picked.FindByPredicate([&Category](const FPicked& P) { return P.Title.Equals(Category.TrimStartAndEnd(), ESearchCase::IgnoreCase); });
		Ideas.Add({ Doc ? Doc->DocId : FString(), Doc ? Doc->Title : FString(), Title, Description.TrimStartAndEnd() });
	}
	if (Ideas.IsEmpty())
	{
		Agent.AddLog(LOCTEXT("LogNoTasks", "The agent didn't suggest any tasks.").ToString());
		return;
	}
	bSucceeded = true;
	Agent.AddLog(FText::Format(LOCTEXT("LogDone", "Done: {0} task {0}|plural(one=idea,other=ideas). Click Next to choose which to keep."), FText::AsNumber(Ideas.Num())).ToString());
}

TArray<FString> FProtoBoyTaskGenerationRun::GetCategoryTitles() const
{
	TArray<FString> Titles;
	for (const FPicked& Doc : Picked)
	{
		Titles.Add(Doc.Title);
	}
	return Titles;
}

void FProtoBoyTaskGenerationRun::AddTasks(const TArray<int32>& Indices, TFunction<void()> OnProgress, TFunction<void(const TArray<FText>& Errors)> OnDone)
{
	UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	IProtoBoyTaskProvider* Tasks = Subsystem ? Subsystem->GetTaskProvider() : nullptr;
	if (bAdding || !Tasks)
	{
		OnDone({ LOCTEXT("NoBoard", "No task board is connected.") });
		return;
	}

	// The doc link and who made it go into each description now.
	const FString AgentName = FProtoBoyMCPClientConfig::GetClientName(Agent.GetClient()).ToString();
	TArray<FProtoBoyNewTask> NewTasks;
	for (const int32 Index : Indices)
	{
		if (!Ideas.IsValidIndex(Index))
		{
			continue;
		}
		FProtoBoyNewTask Task = Ideas[Index];
		if (const FPicked* Doc = Picked.FindByPredicate([&Task](const FPicked& P) { return P.DocId == Task.CategoryId; }))
		{
			Task.Description += FString::Printf(TEXT("\n\n%s"), *ProtoBoyTasks::MakeDesignDocLine(Doc->Path));
		}
		Task.Description = FString::Printf(TEXT("%s\n\n_Generated by %s via ProtoBoy._"), *Task.Description.TrimStart(), *AgentName);
		NewTasks.Add(MoveTemp(Task));
	}
	if (NewTasks.IsEmpty())
	{
		OnDone({});
		return;
	}

	Agent.AddLog(FText::Format(LOCTEXT("LogAdding", "Adding {0} {0}|plural(one=task,other=tasks) to {1}..."), FText::AsNumber(NewTasks.Num()), Tasks->GetDisplayName()).ToString());
	bAdding = true;
	const TSharedRef<TArray<FText>> Errors = MakeShared<TArray<FText>>();
	const TWeakPtr<FProtoBoyTaskGenerationRun> WeakThis = AsShared();
	Tasks->CreateTasks(NewTasks,
		[WeakThis, NewTasks, Errors, OnProgress](int32 Index, const TProtoBoyTaskResult<FProtoBoyTask>& Result)
		{
			const TSharedPtr<FProtoBoyTaskGenerationRun> This = WeakThis.Pin();
			if (!This.IsValid() || This->bCancelled)
			{
				return false;
			}
			if (Result.bOk)
			{
				++This->NumAdded;
				This->Agent.AddLog(FString::Printf(TEXT("Added \"%s\" to \"%s\"."), *Result.Value.Title, *Result.Value.StatusName));
			}
			else
			{
				Errors->Add(FText::Format(LOCTEXT("AddOneFailed", "\"{0}\": {1}"), FText::FromString(NewTasks[Index].Title), Result.Error));
			}
			if (OnProgress)
			{
				OnProgress();
			}
			return true;
		},
		[WeakThis, Errors, OnDone](const TProtoBoyTaskResult<bool>& Result)
		{
			const TSharedPtr<FProtoBoyTaskGenerationRun> This = WeakThis.Pin();
			if (!This.IsValid() || This->bCancelled)
			{
				return;
			}
			This->bAdding = false;
			// A failure before any card (e.g. the board's lists couldn't be read) has no per-task error.
			if (!Result.bOk && Errors->IsEmpty())
			{
				Errors->Add(Result.Error);
			}
			OnDone(*Errors);
		});
}

void FProtoBoyTaskGenerationRun::Cancel()
{
	if (bAdding)
	{
		bCancelled = true;
		bAdding = false;
		Agent.AddLog(FText::Format(LOCTEXT("LogCancelledAdding", "Stopped adding. The {0} {0}|plural(one=task,other=tasks) already added stay on the board."), FText::AsNumber(NumAdded)).ToString());
		return;
	}
	if (bFinished)
	{
		return;
	}
	const bool bWasRunning = IsRunning();
	bCancelled = true;
	Agent.Stop();
	bFinished = true;
	bSucceeded = false;
	if (bWasRunning)
	{
		Agent.AddLog(LOCTEXT("LogCancelled", "Cancelled. No tasks were added.").ToString());
	}
}

#undef LOCTEXT_NAMESPACE
