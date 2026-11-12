#pragma once

#include "MCP/PBMCPConnection.h"

/**
 * The always-loaded part of agent guidance: a ProtoBoy section kept in the project's agent instruction file
 * (CLAUDE.md for Claude Code, GEMINI.md for Gemini CLI). Agents read that file every session, so the workflow
 * applies even before they look at ProtoBoy's tools.
 *
 * Only the managed block (<!-- protoboy:begin agent-guide --> ... end) is touched; the rest of the file is the team's.
 * The file is usually committed, so the guide is shared by everyone on the project.
 */
namespace ProtoBoyAgentGuide
{
	/** Absolute path of the instruction file for this client, or empty if it has none (Claude Desktop isn't project-based). */
	FString GetInstructionFilePath(EProtoBoyMCPClient Client);

	/** Add or refresh the guide in the client's instruction file. True (and no-op) for clients without one. */
	bool WriteForClient(EProtoBoyMCPClient Client, FText& OutError);

	/** The guide's Markdown (inside the managed block). */
	FString GetGuideMarkdown();
}
