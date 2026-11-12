#include "MCP/PBAgentGuide.h"

#include "Docs/PBMarkdown.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "ProtoBoyAgentGuide"

namespace ProtoBoyAgentGuide
{
	const TCHAR* BlockId = TEXT("agent-guide");

	FString GetInstructionFilePath(EProtoBoyMCPClient Client)
	{
		const FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		switch (Client)
		{
		case EProtoBoyMCPClient::ClaudeCode: return FPaths::Combine(ProjectDir, TEXT("CLAUDE.md"));
		case EProtoBoyMCPClient::GeminiCLI: return FPaths::Combine(ProjectDir, TEXT("GEMINI.md"));
		case EProtoBoyMCPClient::ClaudeDesktop: break;
		}
		return FString();
	}

	FString GetGuideMarkdown()
	{
		return TEXT(R"MD(## ProtoBoy: architecture, design docs and tasks

This project uses ProtoBoy (an Unreal Editor plugin) so agents build within the game's architecture instead of growing
disconnected or monolithic code. When the `protoboy` MCP tools are available:

- **Before designing or implementing**, call `get_project_context`, then `locate_feature` to find the system that owns
  the work. Follow the architecture's dependency rules and conventions.
- **New systems or rule exceptions need a decision first**: record it with `adr_create` (or the `record-decision`
  prompt) and get the user's agreement before writing code.
- **Work from tasks**: `task_get` for the description and linked design doc, `task_update_status` when you start and
  finish, `task_comment` to report what changed. Split out-of-scope work into new tasks with `task_create`.
- **Keep docs current**: append to the relevant design doc with `docs_update_section`. Humans own `Architecture.md`
  and existing text; never rewrite them.
- **Prompts**: `plan-feature`, `implement-task`, `record-decision`.

ProtoBoy's tools are only available while the Unreal Editor is open with its MCP server running. Some changes need the
user's approval in the editor; if one is denied, don't retry it, ask the user.
)MD");
	}

	bool WriteForClient(EProtoBoyMCPClient Client, FText& OutError)
	{
		const FString Path = GetInstructionFilePath(Client);
		if (Path.IsEmpty())
		{
			return true;
		}

		FString Text;
		if (FPaths::FileExists(Path) && !FFileHelper::LoadFileToString(Text, *Path))
		{
			OutError = FText::Format(LOCTEXT("ReadFailed", "Couldn't read {0}."), FText::FromString(Path));
			return false;
		}

		const FString Updated = ProtoBoyMarkdown::SetManagedBlock(Text, FString(), BlockId, GetGuideMarkdown());
		if (Updated == Text)
		{
			return true;
		}

		if (!FFileHelper::SaveStringToFile(Updated, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			OutError = FText::Format(LOCTEXT("WriteFailed", "Couldn't write {0}."), FText::FromString(Path));
			return false;
		}
		return true;
	}
}

#undef LOCTEXT_NAMESPACE
