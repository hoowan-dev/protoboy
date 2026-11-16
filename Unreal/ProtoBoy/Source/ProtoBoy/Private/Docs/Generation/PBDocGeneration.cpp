#include "Docs/Generation/PBDocGeneration.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Dom/JsonObject.h"
#include "Docs/PBDocLayers.h"
#include "Docs/PBDocMap.h"
#include "Docs/PBDocTemplates.h"
#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "MCP/PBMCPClientConfig.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PBModule.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "ProtoBoyDocGeneration"

namespace ProtoBoyDocGeneration
{
	constexpr int32 MaxListed = 150;

	FString GetProjectDir()
	{
		FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		FPaths::NormalizeDirectoryName(Dir);
		return Dir;
	}

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

	/** The command line for the agent's CLI, run through the shell with the prompt file on stdin. */
	bool BuildCommand(EProtoBoyMCPClient Client, const FString& PromptFile, FString& OutShell, FString& OutArgs)
	{
		FString Command;
		switch (Client)
		{
		case EProtoBoyMCPClient::ClaudeCode:
			// Read-only tools only; stream-json reports each step as it happens.
			Command = TEXT("claude -p --output-format stream-json --verbose --allowedTools \"Read Glob Grep LS\" --disallowedTools \"Edit Write MultiEdit NotebookEdit Bash WebFetch WebSearch\"");
			break;
		case EProtoBoyMCPClient::GeminiCLI:
			Command = TEXT("gemini -p \"Follow the instructions above.\"");
			break;
		default:
			return false;
		}
#if PLATFORM_WINDOWS
		OutShell = TEXT("cmd.exe");
		OutArgs = FString::Printf(TEXT("/c %s < \"%s\""), *Command, *FPaths::ConvertRelativePathToFull(PromptFile).Replace(TEXT("/"), TEXT("\\")));
#else
		OutShell = TEXT("/bin/sh");
		OutArgs = FString::Printf(TEXT("-lc '%s < \"%s\"'"), *Command.Replace(TEXT("'"), TEXT("'\\''")), *FPaths::ConvertRelativePathToFull(PromptFile));
#endif
		return true;
	}

	/** "Reading Source/Game/Hero.cpp" for a Claude Code tool call. */
	FString DescribeToolUse(const FString& Name, const TSharedPtr<FJsonObject>& Input)
	{
		auto Field = [&Input](const TCHAR* Key) { FString Value; if (Input.IsValid()) { Input->TryGetStringField(Key, Value); } return Value; };
		auto Relative = [](FString Path)
		{
			FPaths::NormalizeFilename(Path);
			const FString Root = GetProjectDir() + TEXT("/");
			return Path.StartsWith(Root, ESearchCase::IgnoreCase) ? Path.Mid(Root.Len()) : Path;
		};
		if (Name == TEXT("Read")) { return FString::Printf(TEXT("Reading %s"), *Relative(Field(TEXT("file_path")))); }
		if (Name == TEXT("Glob")) { return FString::Printf(TEXT("Looking for files: %s"), *Field(TEXT("pattern"))); }
		if (Name == TEXT("Grep")) { return FString::Printf(TEXT("Searching for \"%s\""), *Field(TEXT("pattern"))); }
		if (Name == TEXT("LS")) { return FString::Printf(TEXT("Listing %s"), *Relative(Field(TEXT("path")))); }
		return FString::Printf(TEXT("Using %s"), *Name);
	}

	/** The JSON object in the agent's answer: the last ```json fence, else the outermost braces. */
	FString ExtractJson(const FString& Text)
	{
		const int32 Fence = Text.Find(TEXT("```json"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (Fence != INDEX_NONE)
		{
			const int32 Start = Fence + 7;
			const int32 End = Text.Find(TEXT("```"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start);
			return Text.Mid(Start, End == INDEX_NONE ? MAX_int32 : End - Start).TrimStartAndEnd();
		}
		const int32 First = Text.Find(TEXT("{"));
		const int32 Last = Text.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		return First != INDEX_NONE && Last > First ? Text.Mid(First, Last - First + 1) : FString();
	}
}

FProtoBoyDocGenerationRun::FProtoBoyDocGenerationRun(FProtoBoyDocMap& InMap)
	: Map(InMap)
{
}

FProtoBoyDocGenerationRun::~FProtoBoyDocGenerationRun()
{
	StopProcess();
}

TArray<EProtoBoyMCPClient> FProtoBoyDocGenerationRun::GetSetUpAgents()
{
	FProtoBoyMCPConnection Connection;
	FProtoBoyMCPConnectionStore::Load(Connection);
	TArray<EProtoBoyMCPClient> Agents;
	for (const EProtoBoyMCPClient Client : { EProtoBoyMCPClient::ClaudeCode, EProtoBoyMCPClient::GeminiCLI, EProtoBoyMCPClient::ClaudeDesktop })
	{
		if (Connection.ConfiguredClients.Contains(Client))
		{
			Agents.Add(Client);
		}
	}
	return Agents;
}

bool FProtoBoyDocGenerationRun::CanRun(EProtoBoyMCPClient InClient, FText& OutWhyNot)
{
	if (InClient == EProtoBoyMCPClient::ClaudeDesktop)
	{
		OutWhyNot = LOCTEXT("DesktopCantRun", "Claude Desktop can't run on its own. Use Claude Code or Gemini CLI.");
		return false;
	}
	return true;
}

FString FProtoBoyDocGenerationRun::GetWorkDir()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ProtoBoy") / TEXT("DocGeneration"));
}

bool FProtoBoyDocGenerationRun::Start(EProtoBoyMCPClient InClient, FText& OutError)
{
	if (bRunning || !CanRun(InClient, OutError))
	{
		return false;
	}
	Client = InClient;
	Log.Reset();
	Pending.Reset();
	RawOutput.Reset();
	FinalText.Reset();
	bInJsonFence = bAgentError = bFinished = bSucceeded = false;

	// The prompt goes to the agent's stdin from a file (too long for a command line).
	const FString PromptFile = GetWorkDir() / TEXT("Prompt.md");
	AddLog(LOCTEXT("LogScanning", "Summarizing the Content folder for the agent...").ToString());
	if (!FFileHelper::SaveStringToFile(ProtoBoyDocGeneration::BuildPrompt(), *PromptFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FText::Format(LOCTEXT("PromptWriteFailed", "Couldn't write {0}."), FText::FromString(PromptFile));
		return false;
	}

	FString Shell, Args;
	if (!ProtoBoyDocGeneration::BuildCommand(Client, PromptFile, Shell, Args))
	{
		OutError = LOCTEXT("UnknownAgent", "This agent can't generate docs.");
		return false;
	}

	// Overwrite: the current docs go to the trash before the agent starts.
	AddLog(LOCTEXT("LogClearing", "Moving the current docs to the trash...").ToString());
	if (!Map.ClearDocs(OutError))
	{
		return false;
	}

	if (!FPlatformProcess::CreatePipe(ReadPipe, WritePipe))
	{
		OutError = LOCTEXT("PipeFailed", "Couldn't start the agent (no output pipe).");
		return false;
	}
	Process = FPlatformProcess::CreateProc(*Shell, *Args, /*bLaunchDetached*/ false, /*bLaunchHidden*/ true, /*bLaunchReallyHidden*/ true,
		nullptr, 0, *ProtoBoyDocGeneration::GetProjectDir(), WritePipe, nullptr);
	if (!Process.IsValid())
	{
		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
		ReadPipe = WritePipe = nullptr;
		OutError = FText::Format(LOCTEXT("LaunchFailed", "Couldn't start {0}."), FProtoBoyMCPClientConfig::GetClientName(Client));
		return false;
	}
	bRunning = true;
	AddLog(FText::Format(LOCTEXT("LogStarted", "Started {0}. It reads the project and writes the docs; this can take several minutes."),
		FProtoBoyMCPClientConfig::GetClientName(Client)).ToString());
	return true;
}

void FProtoBoyDocGenerationRun::Tick()
{
	if (!bRunning)
	{
		return;
	}

	// Read what's there, then check for exit, so the last output isn't lost.
	const bool bExited = !FPlatformProcess::IsProcRunning(Process);
	TArray<uint8> Bytes;
	while (FPlatformProcess::ReadPipeToArray(ReadPipe, Bytes) && !Bytes.IsEmpty())
	{
		Pending.Append(Bytes);
		Bytes.Reset();
	}
	int32 LineEnd;
	while (Pending.Find(static_cast<uint8>('\n'), LineEnd))
	{
		const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Pending.GetData()), LineEnd);
		FString Line(Converted.Length(), Converted.Get());
		Pending.RemoveAt(0, LineEnd + 1);
		Line.TrimEndInline();
		RawOutput += Line + TEXT("\n");
		HandleLine(Line);
	}

	if (bExited)
	{
		if (!Pending.IsEmpty())
		{
			const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Pending.GetData()), Pending.Num());
			const FString Line(Converted.Length(), Converted.Get());
			Pending.Reset();
			RawOutput += Line + TEXT("\n");
			HandleLine(Line);
		}
		int32 ReturnCode = 0;
		FPlatformProcess::GetProcReturnCode(Process, &ReturnCode);
		StopProcess();
		Complete(ReturnCode);
	}
}

void FProtoBoyDocGenerationRun::HandleLine(const FString& Line)
{
	if (Line.TrimStartAndEnd().IsEmpty())
	{
		return;
	}
	if (Client == EProtoBoyMCPClient::ClaudeCode && Line.StartsWith(TEXT("{")))
	{
		HandleClaudeEvent(Line);
		return;
	}
	if (Client != EProtoBoyMCPClient::ClaudeCode)
	{
		FinalText += Line + TEXT("\n");
	}
	HandleTextLine(Line);
}

void FProtoBoyDocGenerationRun::HandleClaudeEvent(const FString& Line)
{
	TSharedPtr<FJsonObject> Event;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Line), Event) || !Event.IsValid())
	{
		HandleTextLine(Line);
		return;
	}
	FString Type;
	Event->TryGetStringField(TEXT("type"), Type);
	if (Type == TEXT("assistant"))
	{
		const TSharedPtr<FJsonObject>* Message = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Content = nullptr;
		if (!Event->TryGetObjectField(TEXT("message"), Message) || !(*Message)->TryGetArrayField(TEXT("content"), Content))
		{
			return;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Content)
		{
			const TSharedPtr<FJsonObject> Block = Value->AsObject();
			if (!Block.IsValid())
			{
				continue;
			}
			FString BlockType, Name, Text;
			Block->TryGetStringField(TEXT("type"), BlockType);
			Block->TryGetStringField(TEXT("name"), Name);
			Block->TryGetStringField(TEXT("text"), Text);
			if (BlockType == TEXT("tool_use"))
			{
				const TSharedPtr<FJsonObject>* Input = nullptr;
				Block->TryGetObjectField(TEXT("input"), Input);
				AddLog(ProtoBoyDocGeneration::DescribeToolUse(Name, Input ? *Input : nullptr));
			}
			else if (BlockType == TEXT("text"))
			{
				TArray<FString> Lines;
				Text.ParseIntoArrayLines(Lines);
				for (const FString& TextLine : Lines)
				{
					HandleTextLine(TextLine);
				}
			}
		}
	}
	else if (Type == TEXT("result"))
	{
		Event->TryGetStringField(TEXT("result"), FinalText);
		bool bIsError = false;
		Event->TryGetBoolField(TEXT("is_error"), bIsError);
		bAgentError = bIsError;
		if (bIsError)
		{
			AddLog(FString::Printf(TEXT("The agent reported an error: %s"), *FinalText.Left(500)));
		}
	}
}

void FProtoBoyDocGenerationRun::HandleTextLine(const FString& InLine)
{
	const FString Line = InLine.TrimStartAndEnd();
	if (Line.StartsWith(TEXT("```")))
	{
		const bool bOpening = !bInJsonFence;
		bInJsonFence = !bInJsonFence;
		if (bOpening)
		{
			AddLog(LOCTEXT("LogWriting", "The agent is writing the docs...").ToString());
		}
		return;
	}
	if (bInJsonFence || Line.IsEmpty())
	{
		return;
	}
	FString Progress = Line;
	Progress.RemoveFromStart(TEXT("PROGRESS:"));
	AddLog(Progress.TrimStartAndEnd());
}

void FProtoBoyDocGenerationRun::Complete(int32 ReturnCode)
{
	bRunning = false;
	bFinished = true;
	FFileHelper::SaveStringToFile(RawOutput, *(GetWorkDir() / TEXT("Output.log")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	const FString Json = ProtoBoyDocGeneration::ExtractJson(FinalText);
	if (Json.IsEmpty())
	{
		// 9009 (cmd) / 127 (sh): command not found.
		AddLog(ReturnCode == 9009 || ReturnCode == 127
			? FText::Format(LOCTEXT("LogNotFound", "Couldn't find the {0} command. Install it (and sign in), then try again."), FProtoBoyMCPClientConfig::GetClientName(Client)).ToString()
			: FText::Format(LOCTEXT("LogNoAnswer", "The agent stopped (exit code {0}) without returning the docs. Its output is in {1}."),
				FText::AsNumber(ReturnCode), FText::FromString(GetWorkDir() / TEXT("Output.log"))).ToString());
		return;
	}

	FProtoBoyDocTemplate Template;
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Template))
	{
		AddLog(FText::Format(LOCTEXT("LogBadJson", "The agent's answer isn't valid JSON. Its output is in {0}."), FText::FromString(GetWorkDir() / TEXT("Output.log"))).ToString());
		return;
	}
	Template.Id = TEXT("generated");
	Template.Name = TEXT("Generated");
	Template.bGenerated = true;
	if (const FString Problem = ProtoBoyDocTemplates::Validate(Template); !Problem.IsEmpty())
	{
		AddLog(FString::Printf(TEXT("The agent's docs can't be used: %s."), *Problem));
		return;
	}

	AddLog(FText::Format(LOCTEXT("LogApplying", "Writing {0} docs..."), FText::AsNumber(Template.Docs.Num() + 1)).ToString());
	int32 Created = 0;
	FText Error;
	if (!Map.ApplyTemplate(Template, /*bExamples*/ true, Created, Error))
	{
		AddLog(FText::Format(LOCTEXT("LogApplyFailed", "Couldn't write the docs: {0}"), Error).ToString());
		return;
	}
	bSucceeded = true;
	AddLog(FText::Format(LOCTEXT("LogDone", "Done: wrote {0} docs. They're a starting point: go through them and add the details of your game."), FText::AsNumber(Created)).ToString());
}

void FProtoBoyDocGenerationRun::Cancel()
{
	const bool bWasRunning = bRunning;
	StopProcess();
	bRunning = false;
	bFinished = true;
	bSucceeded = false;
	if (bWasRunning)
	{
		AddLog(LOCTEXT("LogCancelled", "Cancelled.").ToString());
	}
	FText Error;
	if (!Map.ClearDocs(Error))
	{
		UE_LOG(LogProtoBoy, Warning, TEXT("Couldn't clear the docs after cancelling doc generation: %s"), *Error.ToString());
	}
}

void FProtoBoyDocGenerationRun::StopProcess()
{
	if (Process.IsValid())
	{
		if (FPlatformProcess::IsProcRunning(Process))
		{
			FPlatformProcess::TerminateProc(Process, /*KillTree*/ true);
		}
		FPlatformProcess::CloseProc(Process);
		Process.Reset();
	}
	if (ReadPipe || WritePipe)
	{
		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
		ReadPipe = WritePipe = nullptr;
	}
}

void FProtoBoyDocGenerationRun::AddLog(const FString& Line)
{
	if (!Line.IsEmpty())
	{
		Log.Add(Line);
	}
}

#undef LOCTEXT_NAMESPACE
