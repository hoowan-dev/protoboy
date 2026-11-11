#pragma once

#include "MCP/PBMCPConnection.h"

class FJsonObject;

/**
 * Knows how each agent application (MCP client) is configured to reach ProtoBoy's server:
 * where its config lives, what entry to add, and how to add/remove it.
 *
 * Only ProtoBoy's own entry (ServerName) is ever touched. JSON configs are backed up to <file>.protoboy.bak
 * before each write, and a file that fails to parse is left alone.
 */
class PROTOBOY_API FProtoBoyMCPClientConfig
{
public:
	/** Name of the server entry written into client configs. */
	static const TCHAR* ServerName;

	static FText GetProviderName(EProtoBoyLLMProvider Provider);
	/** FProtoBoyStyle brush name for the provider logo. */
	static FName GetProviderIcon(EProtoBoyLLMProvider Provider, bool bLarge = false);
	static TArray<EProtoBoyMCPClient> GetClients(EProtoBoyLLMProvider Provider);

	static FText GetClientName(EProtoBoyMCPClient Client);
	/** One line on how this client is set up (shown under the client name). */
	static FText GetClientHint(EProtoBoyMCPClient Client);

	/** Absolute path of the config file that holds this client's ProtoBoy entry. */
	static FString GetConfigPath(EProtoBoyMCPClient Client);

	/**
	 * Text the user can paste to set up the client manually: a shell command for Claude Code,
	 * a JSON fragment for the others.
	 */
	static FString BuildSetupSnippet(EProtoBoyMCPClient Client, const FString& ServerUrl, const FString& AuthToken);

	/**
	 * Add or replace ProtoBoy's entry in the client's config.
	 * Claude Code goes through its CLI (`claude mcp add`, local scope); the others are JSON merges.
	 * Blocks while the Claude Code CLI runs.
	 */
	static bool WriteConfig(EProtoBoyMCPClient Client, const FString& ServerUrl, const FString& AuthToken, FText& OutError);

	/** Remove ProtoBoy's entry from the client's config. Succeeds if there was no entry. */
	static bool RemoveConfig(EProtoBoyMCPClient Client, FText& OutError);

	/** True if the client's config has a ProtoBoy entry pointing at ServerUrl. Reads the file; don't call per frame. */
	static bool IsConfigured(EProtoBoyMCPClient Client, const FString& ServerUrl);

private:
	/** The value stored under mcpServers.<ServerName> for JSON-configured clients (and for Claude Code's file). */
	static TSharedRef<FJsonObject> MakeServerEntry(EProtoBoyMCPClient Client, const FString& ServerUrl, const FString& AuthToken);

	/** The project's mcpServers object inside ~/.claude.json, if any. */
	static TSharedPtr<FJsonObject> FindClaudeCodeProjectServers(const TSharedPtr<FJsonObject>& Root);

	/** Run `claude <Args>` in the project directory. */
	static bool RunClaudeCli(const FString& Args, FText& OutError);
};
