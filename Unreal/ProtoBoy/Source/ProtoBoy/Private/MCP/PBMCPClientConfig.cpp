#include "MCP/PBMCPClientConfig.h"

#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "ProtoBoyMCPClientConfig"

const TCHAR* FProtoBoyMCPClientConfig::ServerName = TEXT("protoboy");

FText FProtoBoyMCPClientConfig::GetProviderName(EProtoBoyLLMProvider Provider)
{
	switch (Provider)
	{
	case EProtoBoyLLMProvider::Claude: return LOCTEXT("Claude", "Claude");
	case EProtoBoyLLMProvider::Gemini: return LOCTEXT("Gemini", "Gemini");
	}
	return FText::GetEmpty();
}

FName FProtoBoyMCPClientConfig::GetProviderIcon(EProtoBoyLLMProvider Provider, bool bLarge)
{
	switch (Provider)
	{
	case EProtoBoyLLMProvider::Claude: return bLarge ? "ProtoBoy.Icon.Claude.Large" : "ProtoBoy.Icon.Claude";
	case EProtoBoyLLMProvider::Gemini: return bLarge ? "ProtoBoy.Icon.Gemini.Large" : "ProtoBoy.Icon.Gemini";
	}
	return NAME_None;
}

TArray<EProtoBoyMCPClient> FProtoBoyMCPClientConfig::GetClients(EProtoBoyLLMProvider Provider)
{
	switch (Provider)
	{
	case EProtoBoyLLMProvider::Claude: return { EProtoBoyMCPClient::ClaudeCode, EProtoBoyMCPClient::ClaudeDesktop };
	case EProtoBoyLLMProvider::Gemini: return { EProtoBoyMCPClient::GeminiCLI };
	}
	return {};
}

FText FProtoBoyMCPClientConfig::GetClientName(EProtoBoyMCPClient Client)
{
	switch (Client)
	{
	case EProtoBoyMCPClient::ClaudeCode: return LOCTEXT("ClaudeCode", "Claude Code");
	case EProtoBoyMCPClient::ClaudeDesktop: return LOCTEXT("ClaudeDesktop", "Claude Desktop");
	case EProtoBoyMCPClient::GeminiCLI: return LOCTEXT("GeminiCLI", "Gemini CLI");
	}
	return FText::GetEmpty();
}

FText FProtoBoyMCPClientConfig::GetClientHint(EProtoBoyMCPClient Client)
{
	switch (Client)
	{
	case EProtoBoyMCPClient::ClaudeCode:
		return LOCTEXT("ClaudeCodeHint", "Added with local scope, so the token stays out of the project's committed .mcp.json.");
	case EProtoBoyMCPClient::ClaudeDesktop:
		return LOCTEXT("ClaudeDesktopHint", "Desktop launches local servers as commands, so the HTTP server is bridged through mcp-remote (needs Node.js).");
	case EProtoBoyMCPClient::GeminiCLI:
		return LOCTEXT("GeminiCLIHint", "Added to your user settings, so the token stays out of the project's .gemini folder.");
	}
	return FText::GetEmpty();
}

FString FProtoBoyMCPClientConfig::GetConfigPath(EProtoBoyMCPClient Client)
{
	const FString Home = FPlatformProcess::UserHomeDir();
	switch (Client)
	{
	case EProtoBoyMCPClient::ClaudeCode:
		// Local-scope servers live in the user's ~/.claude.json, keyed by project path.
		return FPaths::Combine(Home, TEXT(".claude.json"));
	case EProtoBoyMCPClient::ClaudeDesktop:
		// TODO: macOS is ~/Library/Application Support/Claude/claude_desktop_config.json.
		return FPaths::Combine(FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA")), TEXT("Claude/claude_desktop_config.json"));
	case EProtoBoyMCPClient::GeminiCLI:
		return FPaths::Combine(Home, TEXT(".gemini/settings.json"));
	}
	return FString();
}

FString FProtoBoyMCPClientConfig::BuildSetupSnippet(EProtoBoyMCPClient Client, const FString& ServerUrl, const FString& AuthToken)
{
	switch (Client)
	{
	case EProtoBoyMCPClient::ClaudeCode:
		return FString::Printf(
			TEXT("claude mcp add --transport http --scope local %s %s --header \"Authorization: Bearer %s\""),
			ServerName, *ServerUrl, *AuthToken);

	case EProtoBoyMCPClient::ClaudeDesktop:
		return FString::Printf(TEXT(
			"{\n"
			"  \"mcpServers\": {\n"
			"    \"%s\": {\n"
			"      \"command\": \"npx\",\n"
			"      \"args\": [\"mcp-remote\", \"%s\", \"--header\", \"Authorization: Bearer %s\"]\n"
			"    }\n"
			"  }\n"
			"}"),
			ServerName, *ServerUrl, *AuthToken);

	case EProtoBoyMCPClient::GeminiCLI:
		return FString::Printf(TEXT(
			"{\n"
			"  \"mcpServers\": {\n"
			"    \"%s\": {\n"
			"      \"httpUrl\": \"%s\",\n"
			"      \"headers\": { \"Authorization\": \"Bearer %s\" }\n"
			"    }\n"
			"  }\n"
			"}"),
			ServerName, *ServerUrl, *AuthToken);
	}
	return FString();
}

bool FProtoBoyMCPClientConfig::WriteConfig(EProtoBoyMCPClient Client, const FString& ServerUrl, const FString& AuthToken, FText& OutError)
{
	// TODO: Load GetConfigPath(Client) as JSON (create if missing), set mcpServers.<ServerName> to the entry from
	//       BuildSetupSnippet (Claude Code: projects.<ProjectDir>.mcpServers, or shell out to `claude mcp add`),
	//       write a .bak first, then save. Record the client in FProtoBoyMCPConnection::ConfiguredClients.
	OutError = LOCTEXT("WriteNotImplemented", "Writing client config isn't implemented yet. Use Copy and paste it manually.");
	return false;
}

bool FProtoBoyMCPClientConfig::IsConfigured(EProtoBoyMCPClient Client, const FString& ServerUrl)
{
	// TODO: Parse GetConfigPath(Client) and look for a ServerName entry whose URL matches ServerUrl.
	return false;
}

#undef LOCTEXT_NAMESPACE
