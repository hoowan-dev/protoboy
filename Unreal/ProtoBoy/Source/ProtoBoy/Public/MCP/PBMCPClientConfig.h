#pragma once

#include "MCP/PBMCPConnection.h"

/**
 * Knows how each agent application (MCP client) is configured to reach ProtoBoy's server:
 * where its config lives and what entry to add. Used by the per-provider panels.
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

	/** Absolute path of the config file ProtoBoy would edit for this client. */
	static FString GetConfigPath(EProtoBoyMCPClient Client);

	/**
	 * Text the user can paste to set up the client manually: a shell command for Claude Code,
	 * a JSON fragment for the others.
	 */
	static FString BuildSetupSnippet(EProtoBoyMCPClient Client, const FString& ServerUrl, const FString& AuthToken);

	/** TODO: Merge the ProtoBoy entry into GetConfigPath(Client) without touching other entries; back the file up first. */
	static bool WriteConfig(EProtoBoyMCPClient Client, const FString& ServerUrl, const FString& AuthToken, FText& OutError);

	/** TODO: Parse GetConfigPath(Client) and check for a ProtoBoy entry pointing at ServerUrl. */
	static bool IsConfigured(EProtoBoyMCPClient Client, const FString& ServerUrl);
};
