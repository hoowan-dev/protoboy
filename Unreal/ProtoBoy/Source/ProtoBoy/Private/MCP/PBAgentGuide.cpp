#include "MCP/PBAgentGuide.h"

#include "Docs/PBMarkdown.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PBSettings.h"

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
		const FString Guide = TEXT(R"MD(## ProtoBoy: design docs and tasks

This project uses ProtoBoy (an Unreal Editor plugin) so agents build what the game's docs describe instead of growing
disconnected or monolithic code. The docs are layered: the Game doc (why the game exists, its design pillars, what it
is) > Design docs (each feature, from the player's side) > Tech docs (how each is built, plus shared tech), with Content
docs for what's in the game and its style guides. Every doc ends with a Decisions section. When the `protoboy` MCP tools
are available:

- **Before designing or implementing**, call `get_project_context`, then `locate_feature` to find the Design or shared
  Tech doc the work belongs to. Read its Tech doc and the Decisions sections before changing code. Check the work serves
  one of the Game doc's design pillars and isn't one of its non-goals.
- **New features or systems need a decision first**: propose it, get the user's agreement, then record it with
  `record_decision` in the doc it affects (the Game doc for new scope), or use the `record-decision` prompt.
- **Work from tasks**: `task_get` for the description and linked design doc, `task_update_status` when you start and
  finish, `task_comment` to report what changed. Split out-of-scope work into new tasks with `task_create`.
- **Keep docs current**: record how things are built in the Tech doc, and append to Design docs with
  `docs_update_section`. Humans own the Game doc and all existing text; never rewrite them.
{ASSET_POLICY}- **Prompts**: `plan-feature`, `implement-task`, `record-decision`, `source-assets`.

ProtoBoy's tools are only available while the Unreal Editor is open with its MCP server running. Some changes need the
user's approval in the editor; if one is denied, don't retry it, ask the user.
)MD");
		const FString AssetPolicy = GetDefault<UProtoBoySettings>()->bPreferSourcedAssets
			? FString(TEXT("- **Assets**: don't generate art, audio or other assets with AI. Use the sources in `get_project_context`\n"
				"  (for example `fab_search`) or the `source-assets` prompt, and marked placeholders when nothing fits.\n"))
			: FString();
		return Guide.Replace(TEXT("{ASSET_POLICY}"), *AssetPolicy);
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
