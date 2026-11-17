#include "Docs/Generation/PBDocGeneration.h"

#include "Agents/PBAgentPromptContext.h"
#include "Docs/PBDocLayers.h"
#include "Docs/PBDocMap.h"
#include "Docs/PBDocTemplates.h"
#include "JsonObjectConverter.h"
#include "MCP/PBMCPClientConfig.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PBModule.h"

#define LOCTEXT_NAMESPACE "ProtoBoyDocGeneration"

namespace ProtoBoyDocGeneration
{
	/** "- design: The Player Can, The Player Sees, ..." for every kind a generated doc can have. */
	FString DescribeHeadings()
	{
		FString Out;
		const TPair<const TCHAR*, EProtoBoyDocKind> Kinds[] = {
			{ TEXT("design"), EProtoBoyDocKind::Design },
			{ TEXT("tech"), EProtoBoyDocKind::Tech },
			{ TEXT("content"), EProtoBoyDocKind::Content },
		};
		for (const TPair<const TCHAR*, EProtoBoyDocKind>& Kind : Kinds)
		{
			TArray<FString> Names;
			for (const ProtoBoyDocLayers::FHeading& Heading : ProtoBoyDocLayers::GetHeadings(Kind.Value))
			{
				if (!Heading.Title.Equals(ProtoBoyDocLayers::GetDecisionsHeadingTitle()))
				{
					Names.Add(FString::Printf(TEXT("\"%s\" (%s)"), *Heading.Title, *Heading.Hint));
				}
			}
			Out += FString::Printf(TEXT("- **%s** (%s): %s\n"), Kind.Key, *ProtoBoyDocLayers::GetKindDescription(Kind.Value).ToString(), *FString::Join(Names, TEXT("; ")));
		}
		return Out;
	}

	FString BuildPrompt()
	{
		const FString Project = FApp::GetProjectName();
		return FString::Printf(TEXT(R"MD(# Write the design docs for the Unreal Engine project "%s"

You are drafting the project's design documentation from what is already built. It will be the team's starting point:
they will refine it, so describe what the project actually contains and keep guesses clearly marked.

## Rules

- Read only. Don't create, edit or delete any file, and don't run commands.
- Read the project: the .uproject file, Config/Default*.ini, everything under Source/ (*.Build.cs, *.h, *.cpp) and any
  project plugins under Plugins/ (skip Plugins/ProtoBoy). You can't read .uasset or .umap files; the Content folder is
  summarized below from the editor's asset registry.
- As you work, print one short line per step that starts with "PROGRESS: ", e.g. "PROGRESS: Reading the player
  character's code". Keep them short; they're shown to the user as a progress log.
- Only describe what the project shows. When you infer something (e.g. the genre from asset names), say so ("Looks
  like ..."). Leave a section out rather than inventing it.

## What's in the Content folder

%s
## What to write

The docs have four kinds:

- **game**: one doc, the root. Written from "pillars" and "gameExamples" below.
%s
Docs nest: put each Tech and Content doc under the Design doc it serves (its "parent"), sub-features under their
feature, and leave docs that serve the whole game (save system, audio, UI art) at the top level (empty parent). Aim for
the features, systems and asset groups the project really has: usually 8-40 docs. Ids are lowercase-with-dashes,
unique, and a parent always comes before its children.

Section text is Markdown (bullets, short paragraphs, tables). Design docs say what a player can do and see, in the
player's words; Tech docs name the real classes, modules and data assets you found; Content docs list the real assets
and folders, and describe their style (what to look for and avoid when sourcing more).

## Your answer

When you're done, reply with only this JSON object inside a ```json fence, and nothing after it:

```json
{
  "pillars": [ { "name": "Pillar name", "feeling": "the feeling it gives the player", "rulesOut": "what it rules out" } ],
  "gameExamples": [
    { "heading": "Pitch", "text": "One sentence." },
    { "heading": "Player Fantasy", "text": "..." },
    { "heading": "Core Loop", "text": "1. ...\n2. ..." },
    { "heading": "Non-goals", "text": "- ..." },
    { "heading": "Game Flow", "text": "..." },
    { "heading": "Glossary", "text": "- **Term**: meaning" }
  ],
  "docs": [
    {
      "id": "combat",
      "title": "Combat",
      "kind": "design",
      "parent": "",
      "summary": "One line on what this doc covers.",
      "examples": [ { "heading": "The Player Can", "text": "- As a player, I can ..." } ]
    }
  ]
}
```

No doc may be titled "Uncategorized" (the task board uses that name).

"pillars" are 2-5 design pillars the project seems to be built around. Each doc's "examples" use the exact headings
listed for its kind above; leave out headings you have nothing for.
)MD"), *Project, *ProtoBoyAgentPrompt::DescribeProjectAssets(), *DescribeHeadings());
	}
}

FProtoBoyDocGenerationRun::FProtoBoyDocGenerationRun(FProtoBoyDocMap& InMap)
	: Map(InMap)
{
}

FProtoBoyDocGenerationRun::~FProtoBoyDocGenerationRun()
{
	Agent.Stop();
}

FString FProtoBoyDocGenerationRun::GetWorkDir()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ProtoBoy") / TEXT("DocGeneration"));
}

bool FProtoBoyDocGenerationRun::Start(EProtoBoyMCPClient InClient, FText& OutError)
{
	if (Agent.IsRunning())
	{
		return false;
	}
	bFinished = bSucceeded = false;

	Agent.AddLog(LOCTEXT("LogScanning", "Summarizing the Content folder for the agent...").ToString());
	FProtoBoyAgentRun::FParams Params;
	Params.Client = InClient;
	Params.Prompt = ProtoBoyDocGeneration::BuildPrompt();
	Params.WorkDir = GetWorkDir();
	Params.StartedMessage = FText::Format(LOCTEXT("LogStarted", "Started {0}. It reads the project and writes the docs; this can take several minutes."),
		FProtoBoyMCPClientConfig::GetClientName(InClient));
	Params.AnswerMessage = LOCTEXT("LogWriting", "The agent is writing the docs...");
	// Overwrite: the current docs go to the trash before the agent starts.
	Params.BeforeLaunch = [this](FText& Error)
	{
		Agent.AddLog(LOCTEXT("LogClearing", "Moving the current docs to the trash...").ToString());
		return Map.ClearDocs(Error);
	};
	return Agent.Start(Params, OutError);
}

void FProtoBoyDocGenerationRun::Tick()
{
	if (Agent.Tick())
	{
		Complete();
	}
}

void FProtoBoyDocGenerationRun::Complete()
{
	bFinished = true;

	const FString Json = Agent.ExtractJson();
	if (Json.IsEmpty())
	{
		Agent.AddLog(Agent.DescribeNoAnswer().ToString());
		return;
	}

	FProtoBoyDocTemplate Template;
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Template))
	{
		Agent.AddLog(FText::Format(LOCTEXT("LogBadJson", "The agent's answer isn't valid JSON. Its output is in {0}."), FText::FromString(Agent.GetOutputLogPath())).ToString());
		return;
	}
	Template.Id = TEXT("generated");
	Template.Name = TEXT("Generated");
	Template.bGenerated = true;
	if (const FString Problem = ProtoBoyDocTemplates::Validate(Template); !Problem.IsEmpty())
	{
		Agent.AddLog(FString::Printf(TEXT("The agent's docs can't be used: %s."), *Problem));
		return;
	}

	Agent.AddLog(FText::Format(LOCTEXT("LogApplying", "Writing {0} docs..."), FText::AsNumber(Template.Docs.Num() + 1)).ToString());
	int32 Created = 0;
	FText Error;
	if (!Map.ApplyTemplate(Template, /*bExamples*/ true, Created, Error))
	{
		Agent.AddLog(FText::Format(LOCTEXT("LogApplyFailed", "Couldn't write the docs: {0}"), Error).ToString());
		return;
	}
	bSucceeded = true;
	Agent.AddLog(FText::Format(LOCTEXT("LogDone", "Done: wrote {0} docs. They're a starting point: go through them and add the details of your game."), FText::AsNumber(Created)).ToString());
}

void FProtoBoyDocGenerationRun::Cancel()
{
	const bool bWasRunning = Agent.IsRunning();
	Agent.Stop();
	bFinished = true;
	bSucceeded = false;
	if (bWasRunning)
	{
		Agent.AddLog(LOCTEXT("LogCancelled", "Cancelled.").ToString());
	}
	FText Error;
	if (!Map.ClearDocs(Error))
	{
		UE_LOG(LogProtoBoy, Warning, TEXT("Couldn't clear the docs after cancelling doc generation: %s"), *Error.ToString());
	}
}

#undef LOCTEXT_NAMESPACE
