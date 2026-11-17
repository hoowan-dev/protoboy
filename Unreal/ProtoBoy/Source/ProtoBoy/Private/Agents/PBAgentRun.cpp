#include "Agents/PBAgentRun.h"

#include "Dom/JsonObject.h"
#include "MCP/PBMCPClientConfig.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "ProtoBoyAgentRun"

namespace ProtoBoyAgentRun
{
	/** The command line for the agent's CLI, run through the shell with the prompt file on stdin. */
	bool BuildCommand(EProtoBoyMCPClient Client, const FString& PromptFile, bool bAllowWebSearch, FString& OutShell, FString& OutArgs)
	{
		FString Command;
		switch (Client)
		{
		case EProtoBoyMCPClient::ClaudeCode:
			// Read-only tools only (plus web search when the run asks for it); stream-json reports each step as it happens.
			Command = bAllowWebSearch
				? TEXT("claude -p --output-format stream-json --verbose --allowedTools \"Read Glob Grep LS WebSearch\" --disallowedTools \"Edit Write MultiEdit NotebookEdit Bash WebFetch\"")
				: TEXT("claude -p --output-format stream-json --verbose --allowedTools \"Read Glob Grep LS\" --disallowedTools \"Edit Write MultiEdit NotebookEdit Bash WebFetch WebSearch\"");
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
			const FString Root = FProtoBoyAgentRun::GetProjectDir() + TEXT("/");
			return Path.StartsWith(Root, ESearchCase::IgnoreCase) ? Path.Mid(Root.Len()) : Path;
		};
		if (Name == TEXT("Read")) { return FString::Printf(TEXT("Reading %s"), *Relative(Field(TEXT("file_path")))); }
		if (Name == TEXT("Glob")) { return FString::Printf(TEXT("Looking for files: %s"), *Field(TEXT("pattern"))); }
		if (Name == TEXT("Grep")) { return FString::Printf(TEXT("Searching for \"%s\""), *Field(TEXT("pattern"))); }
		if (Name == TEXT("LS")) { return FString::Printf(TEXT("Listing %s"), *Relative(Field(TEXT("path")))); }
		if (Name == TEXT("WebSearch")) { return FString::Printf(TEXT("Searching the web: %s"), *Field(TEXT("query"))); }
		return FString::Printf(TEXT("Using %s"), *Name);
	}
}

FProtoBoyAgentRun::~FProtoBoyAgentRun()
{
	Stop();
}

TArray<EProtoBoyMCPClient> FProtoBoyAgentRun::GetSetUpAgents()
{
	FProtoBoyMCPConnection Connection;
	FProtoBoyMCPConnectionStore::Load(Connection);
	TArray<EProtoBoyMCPClient> Agents;
	for (const EProtoBoyMCPClient Agent : { EProtoBoyMCPClient::ClaudeCode, EProtoBoyMCPClient::GeminiCLI, EProtoBoyMCPClient::ClaudeDesktop })
	{
		if (Connection.ConfiguredClients.Contains(Agent))
		{
			Agents.Add(Agent);
		}
	}
	return Agents;
}

bool FProtoBoyAgentRun::CanRun(EProtoBoyMCPClient InClient, FText& OutWhyNot)
{
	if (InClient == EProtoBoyMCPClient::ClaudeDesktop)
	{
		OutWhyNot = LOCTEXT("DesktopCantRun", "Claude Desktop can't run on its own. Use Claude Code or Gemini CLI.");
		return false;
	}
	return true;
}

FString FProtoBoyAgentRun::GetProjectDir()
{
	FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeDirectoryName(Dir);
	return Dir;
}

bool FProtoBoyAgentRun::Start(const FParams& Params, FText& OutError)
{
	if (bRunning || !CanRun(Params.Client, OutError))
	{
		return false;
	}
	Client = Params.Client;
	WorkDir = Params.WorkDir;
	AnswerMessage = Params.AnswerMessage;
	Pending.Reset();
	RawOutput.Reset();
	FinalText.Reset();
	bInJsonFence = false;
	ReturnCode = 0;

	// The prompt goes to the agent's stdin from a file (too long for a command line).
	const FString PromptFile = WorkDir / TEXT("Prompt.md");
	if (!FFileHelper::SaveStringToFile(Params.Prompt, *PromptFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FText::Format(LOCTEXT("PromptWriteFailed", "Couldn't write {0}."), FText::FromString(PromptFile));
		return false;
	}
	FString Shell, Args;
	if (!ProtoBoyAgentRun::BuildCommand(Client, PromptFile, Params.bAllowWebSearch, Shell, Args))
	{
		OutError = LOCTEXT("UnknownAgent", "This agent can't run on its own.");
		return false;
	}
	if (Params.BeforeLaunch && !Params.BeforeLaunch(OutError))
	{
		return false;
	}

	if (!FPlatformProcess::CreatePipe(ReadPipe, WritePipe))
	{
		OutError = LOCTEXT("PipeFailed", "Couldn't start the agent (no output pipe).");
		return false;
	}
	Process = FPlatformProcess::CreateProc(*Shell, *Args, /*bLaunchDetached*/ false, /*bLaunchHidden*/ true, /*bLaunchReallyHidden*/ true,
		nullptr, 0, *GetProjectDir(), WritePipe, nullptr);
	if (!Process.IsValid())
	{
		ClosePipes();
		OutError = FText::Format(LOCTEXT("LaunchFailed", "Couldn't start {0}."), FProtoBoyMCPClientConfig::GetClientName(Client));
		return false;
	}
	bRunning = true;
	AddLog(Params.StartedMessage.ToString());
	return true;
}

bool FProtoBoyAgentRun::Tick()
{
	if (!bRunning)
	{
		return false;
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
	if (!bExited)
	{
		return false;
	}

	if (!Pending.IsEmpty())
	{
		const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Pending.GetData()), Pending.Num());
		const FString Line(Converted.Length(), Converted.Get());
		Pending.Reset();
		RawOutput += Line + TEXT("\n");
		HandleLine(Line);
	}
	FPlatformProcess::GetProcReturnCode(Process, &ReturnCode);
	Stop();
	FFileHelper::SaveStringToFile(RawOutput, *GetOutputLogPath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	return true;
}

void FProtoBoyAgentRun::Stop()
{
	bRunning = false;
	if (Process.IsValid())
	{
		if (FPlatformProcess::IsProcRunning(Process))
		{
			FPlatformProcess::TerminateProc(Process, /*KillTree*/ true);
		}
		FPlatformProcess::CloseProc(Process);
		Process.Reset();
	}
	ClosePipes();
}

void FProtoBoyAgentRun::ClosePipes()
{
	if (ReadPipe || WritePipe)
	{
		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
		ReadPipe = WritePipe = nullptr;
	}
}

FString FProtoBoyAgentRun::GetOutputLogPath() const
{
	return WorkDir / TEXT("Output.log");
}

FString FProtoBoyAgentRun::ExtractJson() const
{
	const int32 Fence = FinalText.Find(TEXT("```json"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	if (Fence != INDEX_NONE)
	{
		const int32 Start = Fence + 7;
		const int32 End = FinalText.Find(TEXT("```"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start);
		return FinalText.Mid(Start, End == INDEX_NONE ? MAX_int32 : End - Start).TrimStartAndEnd();
	}
	const int32 First = FinalText.Find(TEXT("{"));
	const int32 Last = FinalText.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	return First != INDEX_NONE && Last > First ? FinalText.Mid(First, Last - First + 1) : FString();
}

FText FProtoBoyAgentRun::DescribeNoAnswer() const
{
	// 9009 (cmd) / 127 (sh): command not found.
	return ReturnCode == 9009 || ReturnCode == 127
		? FText::Format(LOCTEXT("NotFound", "Couldn't find the {0} command. Install it (and sign in), then try again."), FProtoBoyMCPClientConfig::GetClientName(Client))
		: FText::Format(LOCTEXT("NoAnswer", "The agent stopped (exit code {0}) without an answer. Its output is in {1}."),
			FText::AsNumber(ReturnCode), FText::FromString(GetOutputLogPath()));
}

void FProtoBoyAgentRun::HandleLine(const FString& Line)
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

void FProtoBoyAgentRun::HandleClaudeEvent(const FString& Line)
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
				AddLog(ProtoBoyAgentRun::DescribeToolUse(Name, Input ? *Input : nullptr));
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
		if (bIsError)
		{
			AddLog(FString::Printf(TEXT("The agent reported an error: %s"), *FinalText.Left(500)));
		}
	}
}

void FProtoBoyAgentRun::HandleTextLine(const FString& InLine)
{
	const FString Line = InLine.TrimStartAndEnd();
	if (Line.StartsWith(TEXT("```")))
	{
		const bool bOpening = !bInJsonFence;
		bInJsonFence = !bInJsonFence;
		if (bOpening)
		{
			AddLog(AnswerMessage.ToString());
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

void FProtoBoyAgentRun::AddLog(const FString& Line)
{
	if (!Line.IsEmpty())
	{
		Log.Add(Line);
	}
}

#undef LOCTEXT_NAMESPACE
