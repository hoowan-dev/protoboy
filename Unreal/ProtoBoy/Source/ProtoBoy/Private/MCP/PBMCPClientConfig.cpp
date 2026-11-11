#include "MCP/PBMCPClientConfig.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PBModule.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#define LOCTEXT_NAMESPACE "ProtoBoyMCPClientConfig"

const TCHAR* FProtoBoyMCPClientConfig::ServerName = TEXT("protoboy");

namespace ProtoBoyMCPConfig
{
	/** Claude Desktop passes this env var to mcp-remote; see MakeServerEntry. */
	const TCHAR* DesktopAuthEnvVar = TEXT("AUTH_HEADER");

	/**
	 * Load a JSON object file. A missing or empty file yields an empty object.
	 * Returns false (with OutError) if the file exists but isn't a JSON object, so it's never overwritten.
	 */
	bool LoadJsonFile(const FString& Path, TSharedPtr<FJsonObject>& OutRoot, FText& OutError)
	{
		OutRoot = MakeShared<FJsonObject>();

		FString Text;
		if (!FPaths::FileExists(Path) || !FFileHelper::LoadFileToString(Text, *Path) || Text.TrimStartAndEnd().IsEmpty())
		{
			return true;
		}

		TSharedPtr<FJsonObject> Parsed;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Parsed) || !Parsed.IsValid())
		{
			OutError = FText::Format(LOCTEXT("ParseFailed", "Couldn't parse {0}, so it was left unchanged. Fix it or use Copy."),
				FText::FromString(Path));
			return false;
		}

		OutRoot = Parsed;
		return true;
	}

	/** Back up the existing file to <Path>.protoboy.bak, then write Root (pretty-printed). */
	bool SaveJsonFile(const FString& Path, const TSharedRef<FJsonObject>& Root, FText& OutError)
	{
		if (FPaths::FileExists(Path))
		{
			IFileManager::Get().Copy(*(Path + TEXT(".protoboy.bak")), *Path);
		}

		FString Text;
		FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Text));

		if (!FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			OutError = FText::Format(LOCTEXT("WriteFailed", "Couldn't write {0}."), FText::FromString(Path));
			return false;
		}
		return true;
	}

	FString NormalizeDir(FString Dir)
	{
		Dir.ReplaceInline(TEXT("\\"), TEXT("/"));
		while (Dir.EndsWith(TEXT("/")))
		{
			Dir.LeftChopInline(1);
		}
		return Dir;
	}

	FString GetProjectDir()
	{
		return NormalizeDir(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	}
}

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
		return LOCTEXT("ClaudeCodeHint", "Added for this project only (local scope) with the claude CLI, so the token stays out of the committed .mcp.json.");
	case EProtoBoyMCPClient::ClaudeDesktop:
		return LOCTEXT("ClaudeDesktopHint", "Desktop only launches local commands, so ProtoBoy is bridged through mcp-remote (needs Node.js). Fully quit and reopen Desktop after connecting.");
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
		// Local-scope servers live in ~/.claude.json under projects.<project path>.mcpServers.
		return FPaths::Combine(Home, TEXT(".claude.json"));
	case EProtoBoyMCPClient::ClaudeDesktop:
#if PLATFORM_MAC
		return FPaths::Combine(Home, TEXT("Library/Application Support/Claude/claude_desktop_config.json"));
#else
		return FPaths::Combine(FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA")), TEXT("Claude/claude_desktop_config.json"));
#endif
	case EProtoBoyMCPClient::GeminiCLI:
		return FPaths::Combine(Home, TEXT(".gemini/settings.json"));
	}
	return FString();
}

TSharedRef<FJsonObject> FProtoBoyMCPClientConfig::MakeServerEntry(EProtoBoyMCPClient Client, const FString& ServerUrl, const FString& AuthToken)
{
	TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
	const FString Bearer = FString::Printf(TEXT("Bearer %s"), *AuthToken);

	switch (Client)
	{
	case EProtoBoyMCPClient::ClaudeCode:
	{
		Entry->SetStringField(TEXT("type"), TEXT("http"));
		Entry->SetStringField(TEXT("url"), ServerUrl);
		TSharedRef<FJsonObject> Headers = MakeShared<FJsonObject>();
		Headers->SetStringField(TEXT("Authorization"), Bearer);
		Entry->SetObjectField(TEXT("headers"), Headers);
		break;
	}

	case EProtoBoyMCPClient::ClaudeDesktop:
	{
		// Desktop on Windows mangles args containing spaces, so the header value comes from an env var
		// (the header arg itself has no space after the colon). npx is a .cmd script there, so go through cmd /c.
		TArray<TSharedPtr<FJsonValue>> Args;
#if PLATFORM_WINDOWS
		Entry->SetStringField(TEXT("command"), TEXT("cmd"));
		Args.Add(MakeShared<FJsonValueString>(TEXT("/c")));
		Args.Add(MakeShared<FJsonValueString>(TEXT("npx")));
#else
		Entry->SetStringField(TEXT("command"), TEXT("npx"));
#endif
		Args.Add(MakeShared<FJsonValueString>(TEXT("-y")));
		Args.Add(MakeShared<FJsonValueString>(TEXT("mcp-remote")));
		Args.Add(MakeShared<FJsonValueString>(ServerUrl));
		Args.Add(MakeShared<FJsonValueString>(TEXT("--header")));
		Args.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Authorization:${%s}"), ProtoBoyMCPConfig::DesktopAuthEnvVar)));
		Entry->SetArrayField(TEXT("args"), Args);

		TSharedRef<FJsonObject> Env = MakeShared<FJsonObject>();
		Env->SetStringField(ProtoBoyMCPConfig::DesktopAuthEnvVar, Bearer);
		Entry->SetObjectField(TEXT("env"), Env);
		break;
	}

	case EProtoBoyMCPClient::GeminiCLI:
	{
		// httpUrl = Streamable HTTP (url would mean SSE).
		Entry->SetStringField(TEXT("httpUrl"), ServerUrl);
		TSharedRef<FJsonObject> Headers = MakeShared<FJsonObject>();
		Headers->SetStringField(TEXT("Authorization"), Bearer);
		Entry->SetObjectField(TEXT("headers"), Headers);
		break;
	}
	}

	return Entry;
}

FString FProtoBoyMCPClientConfig::BuildSetupSnippet(EProtoBoyMCPClient Client, const FString& ServerUrl, const FString& AuthToken)
{
	if (Client == EProtoBoyMCPClient::ClaudeCode)
	{
		return FString::Printf(
			TEXT("claude mcp add --transport http --scope local %s %s --header \"Authorization: Bearer %s\""),
			ServerName, *ServerUrl, *AuthToken);
	}

	TSharedRef<FJsonObject> Servers = MakeShared<FJsonObject>();
	Servers->SetObjectField(ServerName, MakeServerEntry(Client, ServerUrl, AuthToken));
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetObjectField(TEXT("mcpServers"), Servers);

	FString Text;
	FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Text));
	return Text;
}

bool FProtoBoyMCPClientConfig::WriteConfig(EProtoBoyMCPClient Client, const FString& ServerUrl, const FString& AuthToken, FText& OutError)
{
	if (Client == EProtoBoyMCPClient::ClaudeCode)
	{
		// `add` fails if the name exists, so remove any old entry first (ignoring "not found").
		FText Ignored;
		RunClaudeCli(FString::Printf(TEXT("mcp remove %s --scope local"), ServerName), Ignored);

		return RunClaudeCli(FString::Printf(
			TEXT("mcp add --transport http --scope local %s %s --header \"Authorization: Bearer %s\""),
			ServerName, *ServerUrl, *AuthToken), OutError);
	}

	const FString Path = GetConfigPath(Client);
	TSharedPtr<FJsonObject> Root;
	if (!ProtoBoyMCPConfig::LoadJsonFile(Path, Root, OutError))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* ExistingServers = nullptr;
	TSharedRef<FJsonObject> Servers = Root->TryGetObjectField(TEXT("mcpServers"), ExistingServers) && ExistingServers->IsValid()
		? ExistingServers->ToSharedRef()
		: MakeShared<FJsonObject>();

	Servers->SetObjectField(ServerName, MakeServerEntry(Client, ServerUrl, AuthToken));
	Root->SetObjectField(TEXT("mcpServers"), Servers);

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	return ProtoBoyMCPConfig::SaveJsonFile(Path, Root.ToSharedRef(), OutError);
}

bool FProtoBoyMCPClientConfig::RemoveConfig(EProtoBoyMCPClient Client, FText& OutError)
{
	if (Client == EProtoBoyMCPClient::ClaudeCode)
	{
		if (!IsConfigured(Client, FString()))
		{
			return true;
		}
		return RunClaudeCli(FString::Printf(TEXT("mcp remove %s --scope local"), ServerName), OutError);
	}

	const FString Path = GetConfigPath(Client);
	TSharedPtr<FJsonObject> Root;
	if (!ProtoBoyMCPConfig::LoadJsonFile(Path, Root, OutError))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* Servers = nullptr;
	if (!Root->TryGetObjectField(TEXT("mcpServers"), Servers) || !(*Servers)->HasField(ServerName))
	{
		return true;
	}

	(*Servers)->RemoveField(ServerName);
	return ProtoBoyMCPConfig::SaveJsonFile(Path, Root.ToSharedRef(), OutError);
}

bool FProtoBoyMCPClientConfig::IsConfigured(EProtoBoyMCPClient Client, const FString& ServerUrl)
{
	TSharedPtr<FJsonObject> Root;
	FText Ignored;
	if (!ProtoBoyMCPConfig::LoadJsonFile(GetConfigPath(Client), Root, Ignored))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Servers;
	if (Client == EProtoBoyMCPClient::ClaudeCode)
	{
		Servers = FindClaudeCodeProjectServers(Root);
	}
	else
	{
		const TSharedPtr<FJsonObject>* Found = nullptr;
		if (Root->TryGetObjectField(TEXT("mcpServers"), Found))
		{
			Servers = *Found;
		}
	}

	const TSharedPtr<FJsonObject>* Entry = nullptr;
	if (!Servers.IsValid() || !Servers->TryGetObjectField(ServerName, Entry))
	{
		return false;
	}
	if (ServerUrl.IsEmpty())
	{
		return true;
	}

	// Check the entry points at this server (the port may have changed since it was written).
	switch (Client)
	{
	case EProtoBoyMCPClient::ClaudeCode: return (*Entry)->GetStringField(TEXT("url")) == ServerUrl;
	case EProtoBoyMCPClient::GeminiCLI: return (*Entry)->GetStringField(TEXT("httpUrl")) == ServerUrl;
	case EProtoBoyMCPClient::ClaudeDesktop:
	{
		const TArray<TSharedPtr<FJsonValue>>* Args = nullptr;
		if ((*Entry)->TryGetArrayField(TEXT("args"), Args))
		{
			for (const TSharedPtr<FJsonValue>& Arg : *Args)
			{
				if (Arg.IsValid() && Arg->AsString() == ServerUrl)
				{
					return true;
				}
			}
		}
		return false;
	}
	}
	return false;
}

TSharedPtr<FJsonObject> FProtoBoyMCPClientConfig::FindClaudeCodeProjectServers(const TSharedPtr<FJsonObject>& Root)
{
	const TSharedPtr<FJsonObject>* Projects = nullptr;
	if (!Root.IsValid() || !Root->TryGetObjectField(TEXT("projects"), Projects))
	{
		return nullptr;
	}

	// Keys are absolute project paths; compare normalized (FString == is case-insensitive, matching Windows paths).
	const FString ProjectDir = ProtoBoyMCPConfig::GetProjectDir();
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Projects)->Values)
	{
		const TSharedPtr<FJsonObject>* Project = nullptr;
		const TSharedPtr<FJsonObject>* Servers = nullptr;
		if (ProtoBoyMCPConfig::NormalizeDir(Pair.Key) == ProjectDir
			&& Pair.Value.IsValid() && Pair.Value->TryGetObject(Project)
			&& (*Project)->TryGetObjectField(TEXT("mcpServers"), Servers))
		{
			return *Servers;
		}
	}
	return nullptr;
}

bool FProtoBoyMCPClientConfig::RunClaudeCli(const FString& Args, FText& OutError)
{
	// TODO: Run off the game thread; the CLI takes a second or two to start.
	// Go through the shell so both the native installer (claude.exe) and npm's claude.cmd shim resolve from PATH.
#if PLATFORM_WINDOWS
	const TCHAR* Shell = TEXT("cmd.exe");
	const FString ShellArgs = FString::Printf(TEXT("/c claude %s"), *Args);
#else
	const TCHAR* Shell = TEXT("/bin/sh");
	const FString ShellArgs = FString::Printf(TEXT("-lc 'claude %s'"), *Args);
#endif

	int32 ReturnCode = -1;
	FString StdOut;
	FString StdErr;
	const bool bLaunched = FPlatformProcess::ExecProcess(Shell, *ShellArgs, &ReturnCode, &StdOut, &StdErr, *ProtoBoyMCPConfig::GetProjectDir());

	if (bLaunched && ReturnCode == 0)
	{
		return true;
	}

	// 9009 (cmd) / 127 (sh): command not found.
	if (!bLaunched || ReturnCode == 9009 || ReturnCode == 127)
	{
		OutError = LOCTEXT("ClaudeNotFound", "Couldn't find the claude command. Install Claude Code, or use Copy and run the command yourself.");
	}
	else
	{
		const FString Detail = StdErr.TrimStartAndEnd().IsEmpty() ? StdOut.TrimStartAndEnd() : StdErr.TrimStartAndEnd();
		OutError = FText::Format(LOCTEXT("ClaudeFailed", "claude exited with code {0}: {1}"),
			FText::AsNumber(ReturnCode), FText::FromString(Detail));
	}
	// Args can contain the auth token, so log only the part before the first option (e.g. "mcp add").
	const FString Subcommand = Args.Left(Args.Find(TEXT(" --")));
	UE_LOG(LogProtoBoy, Warning, TEXT("claude %s failed (%d): %s %s"), *Subcommand, ReturnCode, *StdOut, *StdErr);
	return false;
}

#undef LOCTEXT_NAMESPACE
