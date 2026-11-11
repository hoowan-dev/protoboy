#pragma once

#include "MCP/PBMCPConnection.h"

/** A tool ProtoBoy exposes to agents over MCP. */
struct FProtoBoyMCPTool
{
	/** MCP tool name (snake_case). Also the key used in UProtoBoySettings::DisabledMCPTools. */
	FName Name;
	FText Description;
	/** Modifies the project (docs, assets). Write tools are gated by UProtoBoySettings::bAllowMCPWriteTools. */
	bool bWrites = false;

	// TODO: Input JSON schema and a handler, e.g. TFunction<TSharedPtr<FJsonObject>(const TSharedPtr<FJsonObject>& Args)>.
};

/**
 * ProtoBoy as an MCP server: agents (Claude Code, Gemini CLI, ...) connect over local HTTP and call ProtoBoy's tools.
 * Bound to 127.0.0.1 only; clients authenticate with a per-user bearer token.
 * Planned to move into the daemon so one server can serve every engine.
 */
class PROTOBOY_API FProtoBoyMCPServer
{
public:
	FProtoBoyMCPServer();

	/** TODO: Start an HTTP listener (FHttpServerModule) on 127.0.0.1:Port at /mcp, speaking MCP Streamable HTTP. */
	bool Start(int32 Port);
	void Stop();

	bool IsRunning() const { return bRunning; }
	int32 GetPort() const { return Port; }
	/** Endpoint clients connect to, e.g. http://127.0.0.1:47801/mcp (uses the configured port when stopped). */
	FString GetUrl() const;
	int32 GetConnectedClientCount() const { return ConnectedClientCount; }

	const TArray<FProtoBoyMCPTool>& GetTools() const { return Tools; }
	/** Enabled in settings, and allowed by the write-tools setting. */
	bool IsToolEnabled(const FProtoBoyMCPTool& Tool) const;

	/** Returns the saved token, generating and saving one if there is none. */
	const FString& GetAuthToken();
	void RegenerateAuthToken();

	const FProtoBoyMCPConnection& GetConnection() const { return Connection; }

	/** Fires after Start/Stop, client connect/disconnect, and token changes. */
	FSimpleMulticastDelegate OnStatusChanged;

private:
	void RegisterTools();

	TArray<FProtoBoyMCPTool> Tools;
	FProtoBoyMCPConnection Connection;

	bool bRunning = false;
	int32 Port = 0;
	int32 ConnectedClientCount = 0;
};
