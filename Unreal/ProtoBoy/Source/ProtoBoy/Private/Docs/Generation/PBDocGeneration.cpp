#include "Docs/Generation/PBDocGeneration.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Docs/PBDocLayers.h"
#include "Docs/PBDocMap.h"
#include "Docs/PBDocTemplates.h"
#include "JsonObjectConverter.h"
#include "MCP/PBMCPClientConfig.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PBModule.h"

#define LOCTEXT_NAMESPACE "ProtoBoyDocGeneration"

namespace ProtoBoyDocGeneration
{
	constexpr int32 MaxListed = 150;

	/** A Markdown summary of the Content folder from the asset registry: the agent can't read .uasset files itself. */
	FString DescribeContent()
	{
		TArray<FAssetData> Assets;
		IAssetRegistry::GetChecked().GetAssetsByPath(TEXT("/Game"), Assets, /*bRecursive*/ true);
		if (Assets.IsEmpty())
		{
			return TEXT("_The Content folder is empty._\n");
		}
		Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.PackageName.LexicalLess(B.PackageName); });

		TMap<FString, int32> ByClass;
		TMap<FString, int32> ByFolder;
		TArray<FString> Maps;
		TArray<FString> Blueprints;
		TArray<FString> Data;
		for (const FAssetData& Asset : Assets)
		{
			const FString Class = Asset.AssetClassPath.GetAssetName().ToString();
			const FString Path = Asset.PackageName.ToString();
			++ByClass.FindOrAdd(Class);

			// Top two folder levels below /Game, e.g. /Game/Characters/Hero.
			TArray<FString> Parts;
			Path.ParseIntoArray(Parts, TEXT("/"));
			if (Parts.Num() > 2)
			{
				++ByFolder.FindOrAdd(TEXT("/") + Parts[0] + TEXT("/") + Parts[1] + (Parts.Num() > 3 ? TEXT("/") + Parts[2] : FString()));
			}

			if (Class == TEXT("World"))
			{
				Maps.Add(Path);
			}
			else if (Class.EndsWith(TEXT("Blueprint")) || Class == TEXT("Blueprint"))
			{
				FString Parent = Asset.GetTagValueRef<FString>(FName(TEXT("ParentClass")));
				Parent = FPackageName::ObjectPathToObjectName(Parent);
				Parent.RemoveFromEnd(TEXT("'"));
				Blueprints.Add(Parent.IsEmpty() ? Path : FString::Printf(TEXT("%s (%s, parent %s)"), *Path, *Class, *Parent));
			}
			else if (Class == TEXT("DataTable") || Class.Contains(TEXT("DataAsset")) || Class == TEXT("CurveTable") || Class.EndsWith(TEXT("Data")))
			{
				Data.Add(FString::Printf(TEXT("%s (%s)"), *Path, *Class));
			}
		}

		auto List = [](const TArray<FString>& Items) -> FString
		{
			FString Out;
			for (int32 Index = 0; Index < Items.Num() && Index < MaxListed; ++Index)
			{
				Out += FString::Printf(TEXT("- %s\n"), *Items[Index]);
			}
			if (Items.Num() > MaxListed)
			{
				Out += FString::Printf(TEXT("- ...and %d more\n"), Items.Num() - MaxListed);
			}
			return Out.IsEmpty() ? FString(TEXT("_None._\n")) : Out;
		};
		auto Counts = [](const TMap<FString, int32>& Map) -> FString
		{
			TArray<TPair<FString, int32>> Sorted;
			for (const TPair<FString, int32>& Pair : Map)
			{
				Sorted.Add(Pair);
			}
			Sorted.Sort([](const TPair<FString, int32>& A, const TPair<FString, int32>& B) { return A.Value > B.Value; });
			FString Out;
			for (int32 Index = 0; Index < Sorted.Num() && Index < 60; ++Index)
			{
				Out += FString::Printf(TEXT("- %s: %d\n"), *Sorted[Index].Key, Sorted[Index].Value);
			}
			return Out;
		};

		FString Out = FString::Printf(TEXT("%d assets.\n\n### By type\n\n%s\n### By folder\n\n%s\n"), Assets.Num(), *Counts(ByClass), *Counts(ByFolder));
		Out += TEXT("### Maps\n\n") + List(Maps);
		Out += TEXT("\n### Blueprints\n\n") + List(Blueprints);
		Out += TEXT("\n### Data assets and tables\n\n") + List(Data);
		return Out;
	}

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
)MD"), *Project, *DescribeContent(), *DescribeHeadings());
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
