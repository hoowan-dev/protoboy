#pragma once

#include "MCP/PBMCPConnection.h"

class FJsonObject;
class FJsonValue;
class IHttpRouter;
struct FHttpRouteHandleInternal;
struct FHttpServerRequest;
struct FHttpServerResponse;

/** What a tool returns to the agent. Text is Markdown the model reads; bIsError marks a tool execution error. */
struct FProtoBoyMCPToolResult
{
	FString Text;
	bool bIsError = false;

	static FProtoBoyMCPToolResult Ok(const FString& InText) { return { InText, false }; }
	static FProtoBoyMCPToolResult Error(const FString& InText) { return { InText, true }; }
	static FProtoBoyMCPToolResult Error(const FText& InText) { return { InText.ToString(), true }; }
};

/** One tools/call. */
struct FProtoBoyMCPToolCall
{
	/** The call's "arguments" object (never null). */
	TSharedPtr<FJsonObject> Args;
	/** Self-reported agent name, for provenance in ADRs, comments and the activity log. */
	FString ClientName;
};

/** Called exactly once, on the game thread, when the tool finishes (possibly after async work). */
using FProtoBoyMCPToolDone = TFunction<void(const FProtoBoyMCPToolResult&)>;
using FProtoBoyMCPToolHandler = TFunction<void(const FProtoBoyMCPToolCall& Call, FProtoBoyMCPToolDone Done)>;

/** A tool ProtoBoy exposes to agents over MCP. Registered in MCP/PBMCPTools.cpp. */
struct FProtoBoyMCPTool
{
	/** MCP tool name (snake_case). Also the key used in UProtoBoySettings::DisabledMCPTools. */
	FName Name;
	FText Title;
	FText Description;
	/** Changes docs or tasks. Write tools are gated by bAllowMCPWriteTools and, if set, per-call approval. */
	bool bWrites = false;
	/** JSON Schema for the arguments object. */
	TSharedPtr<FJsonObject> InputSchema;
	FProtoBoyMCPToolHandler Handler;
	/** Optional: the tool is only offered while this returns true (e.g. its source is enabled in settings). */
	TFunction<bool()> IsAvailable;
};

/** A workflow prompt (shown by agents as a slash command). Registered in MCP/PBMCPPrompts.cpp. */
struct FProtoBoyMCPPrompt
{
	struct FArgument
	{
		FString Name;
		FString Description;
		bool bRequired = false;
	};

	FString Name;
	FString Title;
	FString Description;
	TArray<FArgument> Arguments;
	/** Builds the prompt text from the given arguments. */
	TFunction<FString(const TMap<FString, FString>& Args)> Build;
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
	 * window (like MCP Settings) is open; UProtoBoySubsystem calls this from Slate's modal loop tick.
	 */
	void TickWhileModal(float DeltaTime);

	const TArray<FProtoBoyMCPTool>& GetTools() const { return Tools; }
	const TArray<FProtoBoyMCPPrompt>& GetPrompts() const { return Prompts; }
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

	/** Rewrite the agent guide in every connected client's instruction file (e.g. after the Architecture doc moves). */
	void RefreshAgentGuides();

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
	using FReplyCallback = TFunction<void(const FReply&)>;

	bool HandleRequest(const FHttpServerRequest& Request, const TFunction<void(TUniquePtr<FHttpServerResponse>&&)>& OnComplete);
	/** Replies through OnReply, immediately or (for tools) once the tool finishes. */
	void HandleMessage(const FHttpServerRequest& Request, const TSharedPtr<FJsonObject>& Message, FReplyCallback OnReply);

	/** Validate a modern (2026-07-28+) request's _meta and mirrored headers. Returns an error reply, or unset if valid. */
	TOptional<FReply> ValidateModernRequest(const FHttpServerRequest& Request, const TSharedPtr<FJsonValue>& Id,
		const FString& Method, const TSharedPtr<FJsonObject>& Params, const FString& Version) const;

	TSharedRef<FJsonObject> MakeInitializeResult(const TSharedPtr<FJsonObject>& Params);
	TSharedRef<FJsonObject> MakeDiscoverResult() const;
	TSharedRef<FJsonObject> MakeToolsListResult() const;
	TSharedRef<FJsonObject> MakePromptsListResult() const;
	/** Null with an error message for an unknown prompt or a missing required argument. */
	TSharedPtr<FJsonObject> MakePromptResult(const TSharedPtr<FJsonObject>& Params, FString& OutError) const;

	/**
	 * Run a tool (after approval, for write tools when confirmation is on). False with OutError for an unknown or
	 * disabled tool; otherwise OnResult is called once with the tool's result.
	 */
	bool CallTool(const TSharedPtr<FJsonObject>& Params, const FString& ClientName, FProtoBoyMCPToolDone OnResult, FString& OutError);

	bool IsAuthorized(const FHttpServerRequest& Request);
	static bool IsAllowedOrigin(const FHttpServerRequest& Request);

	void NoteClient(const FString& ClientName);

	TArray<FProtoBoyMCPTool> Tools;
	TArray<FProtoBoyMCPPrompt> Prompts;
	FProtoBoyMCPConnection Connection;

	/** Cleared on destruction so tool callbacks that finish late don't touch a dead server. */
	TSharedRef<bool> AliveToken = MakeShared<bool>(true);

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
