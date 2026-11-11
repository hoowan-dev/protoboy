#pragma once

#include "MCP/PBMCPConnection.h"

class FJsonObject;
class FJsonValue;
class IHttpRouter;
struct FHttpRouteHandleInternal;
struct FHttpServerRequest;
struct FHttpServerResponse;

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
 *
 * Streamable HTTP on http://127.0.0.1:<port>/mcp, using the engine's HTTPServer module (requests are handled on the
 * game thread, so tools can touch UObjects). Dual-era: serves modern stateless requests (protocol 2026-07-28, per-request
 * _meta) and legacy clients that start with an `initialize` handshake (2025-03-26 .. 2025-11-25). Every request must
 * carry the per-user bearer token; browser origins other than localhost are rejected.
 *
 * Planned to move into the daemon so one server can serve every engine.
 */
class PROTOBOY_API FProtoBoyMCPServer
{
public:
	FProtoBoyMCPServer();
	~FProtoBoyMCPServer();

	/** Bind /mcp on 127.0.0.1:Port and start listening. On failure returns false with a reason in OutError. */
	bool Start(int32 Port, FText* OutError = nullptr);

	/**
	 * Unbind the /mcp route. The engine keeps the port's listener open (it has no per-port stop),
	 * so the port stays bound but answers 404 until the next Start.
	 */
	void Stop();

	bool IsRunning() const { return bRunning; }
	int32 GetPort() const { return Port; }
	/** Endpoint clients connect to, e.g. http://127.0.0.1:47801/mcp (uses the configured port when stopped). */
	FString GetUrl() const;

	/** Agents (by self-reported client name) that made a request in the last few minutes. MCP is stateless, so this is the closest thing to "connected". */
	TArray<FString> GetRecentClients() const;
	int32 GetConnectedClientCount() const { return GetRecentClients().Num(); }

	/**
	 * Pump the listener directly. The engine ticks HTTP listeners from the core ticker, which stalls while a modal
	 * window (like MCP Settings) is open; call this from such a window's Tick so agents still get answers.
	 */
	void TickWhileModal(float DeltaTime);

	const TArray<FProtoBoyMCPTool>& GetTools() const { return Tools; }
	/** Enabled in settings, and allowed by the write-tools setting. */
	bool IsToolEnabled(const FProtoBoyMCPTool& Tool) const;

	/** Returns the saved token, generating and saving one if there is none. */
	const FString& GetAuthToken();
	/** New token; re-writes the config of every connected client so they keep working. */
	void RegenerateAuthToken();

	/**
	 * One-click setup: write ProtoBoy's entry (URL + token) into the client's config, remember the client,
	 * and start the server if it isn't running.
	 */
	bool ConnectClient(EProtoBoyMCPClient Client, FText& OutError);
	/** Remove ProtoBoy's entry from the client's config and forget the client. */
	bool DisconnectClient(EProtoBoyMCPClient Client, FText& OutError);

	/** Any client connected by this user. The server then starts with the editor for them even if the team setting is off. */
	bool HasConnectedClients() const { return Connection.ConfiguredClients.Num() > 0; }

	const FProtoBoyMCPConnection& GetConnection() const { return Connection; }

	/** Fires after Start/Stop, a new agent showing up, and token changes. */
	FSimpleMulticastDelegate OnStatusChanged;

private:
	/** A JSON-RPC reply: HTTP status plus body (empty body = no content). */
	struct FReply
	{
		int32 HttpStatus = 200;
		TSharedPtr<FJsonObject> Body;
	};

	void RegisterTools();

	bool HandleRequest(const FHttpServerRequest& Request, const TFunction<void(TUniquePtr<FHttpServerResponse>&&)>& OnComplete);
	FReply HandleMessage(const FHttpServerRequest& Request, const TSharedPtr<FJsonObject>& Message);

	/** Validate a modern (2026-07-28+) request's _meta and mirrored headers. Returns an error reply, or unset if valid. */
	TOptional<FReply> ValidateModernRequest(const FHttpServerRequest& Request, const TSharedPtr<FJsonValue>& Id,
		const FString& Method, const TSharedPtr<FJsonObject>& Params, const FString& Version) const;

	TSharedRef<FJsonObject> MakeInitializeResult(const TSharedPtr<FJsonObject>& Params);
	TSharedRef<FJsonObject> MakeDiscoverResult() const;
	TSharedRef<FJsonObject> MakeToolsListResult() const;
	/** Returns unset with an error reply in OutError for an unknown/disabled tool. */
	TSharedPtr<FJsonObject> CallTool(const TSharedPtr<FJsonObject>& Params, const TSharedPtr<FJsonValue>& Id, FReply& OutError);

	bool IsAuthorized(const FHttpServerRequest& Request);
	static bool IsAllowedOrigin(const FHttpServerRequest& Request);

	void NoteClient(const FString& ClientName);

	TArray<FProtoBoyMCPTool> Tools;
	FProtoBoyMCPConnection Connection;

	TSharedPtr<IHttpRouter> Router;
	/** FHttpRouteHandle, spelled out so this header doesn't need the HTTPServer module. */
	TSharedPtr<const FHttpRouteHandleInternal> RouteHandle;
	bool bRunning = false;
	int32 Port = 0;

	/** Client name -> last request time (FPlatformTime::Seconds). */
	TMap<FString, double> ClientLastSeen;
	/** Name from the most recent legacy initialize; legacy requests after it carry no client name. */
	FString LastLegacyClient;
};
