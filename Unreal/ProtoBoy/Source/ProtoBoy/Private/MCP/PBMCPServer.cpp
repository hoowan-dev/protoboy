#include "MCP/PBMCPServer.h"

#include "Dom/JsonObject.h"
#include "HttpPath.h"
#include "HttpServerModule.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"
#include "Interfaces/IPluginManager.h"
#include "MCP/PBAgentActivityLog.h"
#include "MCP/PBAgentGuide.h"
#include "MCP/PBMCPApprovals.h"
#include "MCP/PBMCPClientConfig.h"
#include "MCP/PBMCPPrompts.h"
#include "MCP/PBMCPTools.h"
#include "Net/PBHttpServerUtils.h"
#include "Misc/Base64.h"
#include "Misc/Guid.h"
#include "Modules/ModuleManager.h"
#include "PBModule.h"
#include "PBSettings.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#define LOCTEXT_NAMESPACE "ProtoBoyMCPServer"

namespace ProtoBoyMCP
{
	/** Modern (stateless, per-request _meta) protocol versions we serve. */
	const TCHAR* ModernVersions[] = { TEXT("2026-07-28") };
	/** Legacy (initialize handshake) versions we serve; first is preferred when negotiating. */
	const TCHAR* LegacyVersions[] = { TEXT("2025-11-25"), TEXT("2025-06-18"), TEXT("2025-03-26") };

	const TCHAR* MetaProtocolVersion = TEXT("io.modelcontextprotocol/protocolVersion");
	const TCHAR* MetaClientInfo = TEXT("io.modelcontextprotocol/clientInfo");
	const TCHAR* MetaClientCapabilities = TEXT("io.modelcontextprotocol/clientCapabilities");
	const TCHAR* MetaServerInfo = TEXT("io.modelcontextprotocol/serverInfo");

	// JSON-RPC / MCP error codes.
	constexpr int32 ParseError = -32700;
	constexpr int32 InvalidRequest = -32600;
	constexpr int32 MethodNotFound = -32601;
	constexpr int32 InvalidParams = -32602;
	constexpr int32 HeaderMismatch = -32020;
	constexpr int32 UnsupportedProtocolVersion = -32022;

	/** How long after its last request an agent still counts as connected. */
	constexpr double RecentClientSeconds = 5.0 * 60.0;

	bool IsModernVersion(const FString& Version)
	{
		for (const TCHAR* V : ModernVersions) { if (Version.Equals(V, ESearchCase::CaseSensitive)) return true; }
		return false;
	}

	bool IsLegacyVersion(const FString& Version)
	{
		for (const TCHAR* V : LegacyVersions) { if (Version.Equals(V, ESearchCase::CaseSensitive)) return true; }
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> AllSupportedVersions()
	{
		TArray<TSharedPtr<FJsonValue>> Out;
		for (const TCHAR* V : ModernVersions) { Out.Add(MakeShared<FJsonValueString>(V)); }
		for (const TCHAR* V : LegacyVersions) { Out.Add(MakeShared<FJsonValueString>(V)); }
		return Out;
	}

	/** Decode the `=?base64?...?=` sentinel used for non-ASCII Mcp-Name values. */
	FString DecodeHeaderValue(const FString& Value)
	{
		const TCHAR* Prefix = TEXT("=?base64?");
		const TCHAR* Suffix = TEXT("?=");
		if (Value.StartsWith(Prefix, ESearchCase::CaseSensitive) && Value.EndsWith(Suffix, ESearchCase::CaseSensitive))
		{
			const FString Encoded = Value.Mid(FCString::Strlen(Prefix), Value.Len() - FCString::Strlen(Prefix) - FCString::Strlen(Suffix));
			TArray<uint8> Bytes;
			if (FBase64::Decode(Encoded, Bytes))
			{
				const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
				return FString(Converted.Length(), Converted.Get());
			}
		}
		return Value;
	}

	TSharedRef<FJsonObject> MakeRpcError(const TSharedPtr<FJsonValue>& Id, int32 Code, const FString& Message,
		const TSharedPtr<FJsonObject>& Data = nullptr)
	{
		TSharedRef<FJsonObject> Error = MakeShared<FJsonObject>();
		Error->SetNumberField(TEXT("code"), Code);
		Error->SetStringField(TEXT("message"), Message);
		if (Data.IsValid())
		{
			Error->SetObjectField(TEXT("data"), Data);
		}

		TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
		Response->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
		if (Id.IsValid())
		{
			Response->SetField(TEXT("id"), Id);
		}
		Response->SetObjectField(TEXT("error"), Error);
		return Response;
	}

	TSharedRef<FJsonObject> MakeResult(const TSharedPtr<FJsonValue>& Id, const TSharedRef<FJsonObject>& Result)
	{
		TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
		Response->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
		Response->SetField(TEXT("id"), Id);
		Response->SetObjectField(TEXT("result"), Result);
		return Response;
	}

	TSharedRef<FJsonObject> MakeServerInfo()
	{
		FString Version = TEXT("0.0.0");
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ProtoBoy")))
		{
			Version = Plugin->GetDescriptor().VersionName;
		}

		TSharedRef<FJsonObject> Info = MakeShared<FJsonObject>();
		Info->SetStringField(TEXT("name"), TEXT("protoboy"));
		Info->SetStringField(TEXT("title"), TEXT("ProtoBoy"));
		Info->SetStringField(TEXT("version"), Version);
		return Info;
	}

	const TCHAR* Instructions = TEXT(
		"ProtoBoy connects you to this game project's architecture, design docs and task board, from inside the Unreal Editor. "
		"Before designing or implementing anything, call get_project_context, then locate_feature to find the system that should own the work. "
		"Follow the architecture's dependency rules and conventions. A new system or module, or an exception to a rule, needs an ADR (adr_create) and the user's agreement first. "
		"Work from tasks: task_get for details and the linked design doc, task_update_status as you start and finish, task_comment to report what changed. "
		"Put out-of-scope work into new tasks (task_create) instead of growing the current one. "
		"Keep docs current by appending to design docs (docs_update_section). Humans own the Architecture doc (get_project_context gives its path) and existing text: never rewrite them. "
		"Write tools can require the user's approval in the editor; if a change is denied, don't retry it, ask the user. "
		"Don't generate assets with AI when the project prefers sourced assets: check the Asset sourcing section of get_project_context. "
		"Workflow prompts: plan-feature, implement-task, record-decision, source-assets.");

	/** Compact JSON of a tool's arguments, cut to a readable length (approvals, activity log). */
	FString SummarizeArgs(const TSharedPtr<FJsonObject>& Args)
	{
		FString Text;
		if (Args.IsValid())
		{
			FJsonSerializer::Serialize(Args.ToSharedRef(), TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
		}
		return Text.Len() > 400 ? Text.Left(400) + TEXT("...") : Text;
	}

	TUniquePtr<FHttpServerResponse> MakeHttpResponse(int32 Status, const TSharedPtr<FJsonObject>& Body)
	{
		TUniquePtr<FHttpServerResponse> Response;
		if (Body.IsValid())
		{
			FString Text;
			FJsonSerializer::Serialize(Body.ToSharedRef(), TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
			Response = FHttpServerResponse::Create(Text, TEXT("application/json"));
		}
		else
		{
			Response = MakeUnique<FHttpServerResponse>();
		}
		Response->Code = static_cast<EHttpServerResponseCodes>(Status);
		return Response;
	}
}

FProtoBoyMCPServer::FProtoBoyMCPServer()
{
	FProtoBoyMCPConnectionStore::Load(Connection);
	ProtoBoyMCPTools::RegisterTools(Tools);
	ProtoBoyMCPPrompts::RegisterPrompts(Prompts);
}

FProtoBoyMCPServer::~FProtoBoyMCPServer()
{
	*AliveToken = false;
	AliveToken = MakeShared<bool>(false);
	Stop();
}

bool FProtoBoyMCPServer::Start(int32 InPort, FText* OutError)
{
	if (bRunning)
	{
		return true;
	}

	auto Fail = [OutError](const FText& Error)
	{
		UE_LOG(LogProtoBoy, Warning, TEXT("MCP server: %s"), *Error.ToString());
		if (OutError)
		{
			*OutError = Error;
		}
		return false;
	};

	if (!FModuleManager::Get().LoadModule(TEXT("HTTPServer")))
	{
		return Fail(LOCTEXT("NoHttpModule", "The engine's HTTPServer module couldn't be loaded."));
	}

	FHttpServerModule& HttpServer = FHttpServerModule::Get();

	// Loopback only, whatever [HTTPServer.Listeners] DefaultBindAddress says. Must precede the listener's creation.
	HttpServer.SetListenerBindAddress(InPort, TEXT("127.0.0.1"));

	Router = HttpServer.GetHttpRouter(InPort, /*bFailOnBindFailure*/ true);
	if (!Router.IsValid())
	{
		return Fail(FText::Format(LOCTEXT("BindFailed", "Couldn't listen on port {0}. Another program may be using it; change the port in Project Settings."),
			FText::AsNumber(InPort, &FNumberFormattingOptions::DefaultNoGrouping())));
	}

	RouteHandle = Router->BindRoute(FHttpPath(TEXT("/mcp")),
		EHttpServerRequestVerbs::VERB_POST | EHttpServerRequestVerbs::VERB_GET | EHttpServerRequestVerbs::VERB_DELETE,
		FHttpRequestHandler::CreateRaw(this, &FProtoBoyMCPServer::HandleRequest));
	if (!RouteHandle.IsValid())
	{
		Router.Reset();
		return Fail(FText::Format(LOCTEXT("RouteTaken", "Port {0} already has an /mcp endpoint from another plugin."),
			FText::AsNumber(InPort, &FNumberFormattingOptions::DefaultNoGrouping())));
	}

	HttpServer.StartAllListeners();

	bRunning = true;
	Port = InPort;
	UE_LOG(LogProtoBoy, Log, TEXT("MCP server listening on %s"), *GetUrl());
	OnStatusChanged.Broadcast();
	return true;
}

void FProtoBoyMCPServer::Stop()
{
	if (!bRunning)
	{
		return;
	}

	if (Router.IsValid() && RouteHandle.IsValid() && FHttpServerModule::IsAvailable())
	{
		Router->UnbindRoute(RouteHandle);
	}
	RouteHandle.Reset();
	Router.Reset();

	bRunning = false;
	Port = 0;
	ClientLastSeen.Reset();
	LastLegacyClient.Reset();
	UE_LOG(LogProtoBoy, Log, TEXT("MCP server stopped."));
	OnStatusChanged.Broadcast();
}

FString FProtoBoyMCPServer::GetUrl() const
{
	const int32 UrlPort = bRunning ? Port : GetDefault<UProtoBoySettings>()->MCPPort;
	return FString::Printf(TEXT("http://127.0.0.1:%d/mcp"), UrlPort);
}

TArray<FString> FProtoBoyMCPServer::GetRecentClients() const
{
	const double Now = FPlatformTime::Seconds();
	TArray<FString> Recent;
	for (const TPair<FString, double>& Pair : ClientLastSeen)
	{
		if (Now - Pair.Value <= ProtoBoyMCP::RecentClientSeconds)
		{
			Recent.Add(Pair.Key);
		}
	}
	return Recent;
}

void FProtoBoyMCPServer::TickWhileModal(float DeltaTime)
{
	if (bRunning && FHttpServerModule::IsAvailable())
	{
		FHttpServerModule::Get().TickListener(Port, DeltaTime);
	}
}

bool FProtoBoyMCPServer::IsToolEnabled(const FProtoBoyMCPTool& Tool) const
{
	const UProtoBoySettings* Settings = GetDefault<UProtoBoySettings>();
	if (Tool.bWrites && !Settings->bAllowMCPWriteTools)
	{
		return false;
	}
	if (Tool.IsAvailable && !Tool.IsAvailable())
	{
		return false;
	}
	return !Settings->DisabledMCPTools.Contains(Tool.Name);
}

// ---------------------------------------------------------------------------------------------------------------------
// HTTP / JSON-RPC

bool FProtoBoyMCPServer::HandleRequest(const FHttpServerRequest& Request, const TFunction<void(TUniquePtr<FHttpServerResponse>&&)>& OnComplete)
{
	// DNS-rebinding protection: a browser page on another origin must not reach the editor.
	if (!IsAllowedOrigin(Request))
	{
		OnComplete(ProtoBoyMCP::MakeHttpResponse(403, ProtoBoyMCP::MakeRpcError(nullptr, ProtoBoyMCP::InvalidRequest, TEXT("Origin not allowed"))));
		return true;
	}

	if (!IsAuthorized(Request))
	{
		TUniquePtr<FHttpServerResponse> Response = ProtoBoyMCP::MakeHttpResponse(401, ProtoBoyMCP::MakeRpcError(nullptr, ProtoBoyMCP::InvalidRequest, TEXT("Missing or invalid bearer token")));
		Response->Headers.Add(TEXT("WWW-Authenticate"), { TEXT("Bearer") });
		OnComplete(MoveTemp(Response));
		return true;
	}

	// No standalone SSE stream (GET) and no sessions to end (DELETE).
	if (Request.Verb != EHttpServerRequestVerbs::VERB_POST)
	{
		TUniquePtr<FHttpServerResponse> Response = ProtoBoyMCP::MakeHttpResponse(405, nullptr);
		Response->Headers.Add(TEXT("Allow"), { TEXT("POST") });
		OnComplete(MoveTemp(Response));
		return true;
	}

	const FString Body = ProtoBoyHttp::GetBodyAsString(Request);

	TSharedPtr<FJsonObject> Message;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Message) || !Message.IsValid())
	{
		OnComplete(ProtoBoyMCP::MakeHttpResponse(400, ProtoBoyMCP::MakeRpcError(nullptr, ProtoBoyMCP::ParseError, TEXT("Body must be a single JSON-RPC object"))));
		return true;
	}

	// The reply may come later (tools that call Trello, or wait for the user's approval).
	HandleMessage(Request, Message, [OnComplete](const FReply& Reply)
	{
		OnComplete(ProtoBoyMCP::MakeHttpResponse(Reply.HttpStatus, Reply.Body));
	});
	return true;
}

void FProtoBoyMCPServer::HandleMessage(const FHttpServerRequest& Request, const TSharedPtr<FJsonObject>& Message, FReplyCallback OnReply)
{
	const TSharedPtr<FJsonValue> Id = Message->TryGetField(TEXT("id"));
	FString Method;
	if (!Message->TryGetStringField(TEXT("method"), Method))
	{
		// Clients don't send responses to servers in any version we speak.
		OnReply({ 400, ProtoBoyMCP::MakeRpcError(Id, ProtoBoyMCP::InvalidRequest, TEXT("Expected a JSON-RPC request or notification")) });
		return;
	}

	const TSharedPtr<FJsonObject>* ParamsPtr = nullptr;
	const TSharedPtr<FJsonObject> Params = Message->TryGetObjectField(TEXT("params"), ParamsPtr) ? *ParamsPtr : MakeShared<FJsonObject>();

	// Notifications (e.g. legacy notifications/initialized) need no reply.
	if (!Id.IsValid() || Id->IsNull())
	{
		OnReply({ 202, nullptr });
		return;
	}

	// Era detection: modern requests carry their protocol version in params._meta.
	const TSharedPtr<FJsonObject>* MetaPtr = nullptr;
	FString ModernVersion;
	const bool bModern = Params->TryGetObjectField(TEXT("_meta"), MetaPtr)
		&& (*MetaPtr)->TryGetStringField(ProtoBoyMCP::MetaProtocolVersion, ModernVersion);

	FString ClientName;
	if (bModern)
	{
		if (TOptional<FReply> Invalid = ValidateModernRequest(Request, Id, Method, Params, ModernVersion))
		{
			OnReply(Invalid.GetValue());
			return;
		}

		const TSharedPtr<FJsonObject>* ClientInfo = nullptr;
		if (!(*MetaPtr)->TryGetObjectField(ProtoBoyMCP::MetaClientInfo, ClientInfo) || !(*ClientInfo)->TryGetStringField(TEXT("name"), ClientName))
		{
			ClientName = TEXT("Unknown client");
		}
		NoteClient(ClientName);
	}
	else
	{
		// Legacy: after initialize, requests name their version in the header only (absent = 2025-03-26).
		const FString HeaderVersion = ProtoBoyHttp::GetHeader(Request, TEXT("MCP-Protocol-Version"));
		if (Method != TEXT("initialize") && !HeaderVersion.IsEmpty() && !ProtoBoyMCP::IsLegacyVersion(HeaderVersion))
		{
			if (ProtoBoyMCP::IsModernVersion(HeaderVersion))
			{
				OnReply({ 400, ProtoBoyMCP::MakeRpcError(Id, ProtoBoyMCP::InvalidParams, FString::Printf(TEXT("Missing params._meta[\"%s\"]"), ProtoBoyMCP::MetaProtocolVersion)) });
				return;
			}
			TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
			Data->SetArrayField(TEXT("supported"), ProtoBoyMCP::AllSupportedVersions());
			Data->SetStringField(TEXT("requested"), HeaderVersion);
			OnReply({ 400, ProtoBoyMCP::MakeRpcError(Id, ProtoBoyMCP::UnsupportedProtocolVersion, TEXT("Unsupported protocol version"), Data) });
			return;
		}
		// Legacy requests only name the client in initialize; credit later requests to the last one seen.
		ClientName = LastLegacyClient.IsEmpty() ? FString(TEXT("Unknown client")) : LastLegacyClient;
		if (Method != TEXT("initialize"))
		{
			NoteClient(ClientName);
		}
	}

	// Wraps a result object in a JSON-RPC response (modern results also carry resultType and serverInfo).
	auto Complete = [Id, bModern](const TSharedRef<FJsonObject>& Result) -> FReply
	{
		if (bModern)
		{
			Result->SetStringField(TEXT("resultType"), TEXT("complete"));
			TSharedRef<FJsonObject> ResultMeta = MakeShared<FJsonObject>();
			ResultMeta->SetObjectField(ProtoBoyMCP::MetaServerInfo, ProtoBoyMCP::MakeServerInfo());
			Result->SetObjectField(TEXT("_meta"), ResultMeta);
		}
		return { 200, ProtoBoyMCP::MakeResult(Id, Result) };
	};

	if (Method == TEXT("initialize") && !bModern)
	{
		OnReply(Complete(MakeInitializeResult(Params)));
	}
	else if (Method == TEXT("server/discover") && bModern)
	{
		OnReply(Complete(MakeDiscoverResult()));
	}
	else if (Method == TEXT("ping"))
	{
		OnReply(Complete(MakeShared<FJsonObject>()));
	}
	else if (Method == TEXT("tools/list"))
	{
		OnReply(Complete(MakeToolsListResult()));
	}
	else if (Method == TEXT("prompts/list"))
	{
		OnReply(Complete(MakePromptsListResult()));
	}
	else if (Method == TEXT("prompts/get"))
	{
		FString Error;
		const TSharedPtr<FJsonObject> Result = MakePromptResult(Params, Error);
		OnReply(Result.IsValid()
			? Complete(Result.ToSharedRef())
			: FReply{ bModern ? 400 : 200, ProtoBoyMCP::MakeRpcError(Id, ProtoBoyMCP::InvalidParams, Error) });
	}
	else if (Method == TEXT("tools/call"))
	{
		FString Error;
		const TWeakPtr<bool> Alive = AliveToken;
		const bool bStarted = CallTool(Params, ClientName,
			[Alive, Complete, OnReply](const FProtoBoyMCPToolResult& ToolResult)
			{
				if (!Alive.IsValid())
				{
					return;
				}

				TSharedRef<FJsonObject> Text = MakeShared<FJsonObject>();
				Text->SetStringField(TEXT("type"), TEXT("text"));
				Text->SetStringField(TEXT("text"), ToolResult.Text);
				TArray<TSharedPtr<FJsonValue>> Content;
				Content.Add(MakeShared<FJsonValueObject>(Text));

				TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
				Result->SetArrayField(TEXT("content"), Content);
				Result->SetBoolField(TEXT("isError"), ToolResult.bIsError);
				OnReply(Complete(Result));
			},
			Error);

		if (!bStarted)
		{
			OnReply({ 200, ProtoBoyMCP::MakeRpcError(Id, ProtoBoyMCP::InvalidParams, Error) });
		}
	}
	else
	{
		OnReply({ bModern ? 404 : 200, ProtoBoyMCP::MakeRpcError(Id, ProtoBoyMCP::MethodNotFound, FString::Printf(TEXT("Method not found: %s"), *Method)) });
	}
}

TOptional<FProtoBoyMCPServer::FReply> FProtoBoyMCPServer::ValidateModernRequest(const FHttpServerRequest& Request,
	const TSharedPtr<FJsonValue>& Id, const FString& Method, const TSharedPtr<FJsonObject>& Params, const FString& Version) const
{
	if (!ProtoBoyMCP::IsModernVersion(Version))
	{
		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetArrayField(TEXT("supported"), ProtoBoyMCP::AllSupportedVersions());
		Data->SetStringField(TEXT("requested"), Version);
		return FReply{ 400, ProtoBoyMCP::MakeRpcError(Id, ProtoBoyMCP::UnsupportedProtocolVersion, TEXT("Unsupported protocol version"), Data) };
	}

	const TSharedPtr<FJsonObject> Meta = Params->GetObjectField(TEXT("_meta"));
	if (!Meta->HasTypedField<EJson::Object>(ProtoBoyMCP::MetaClientCapabilities))
	{
		return FReply{ 400, ProtoBoyMCP::MakeRpcError(Id, ProtoBoyMCP::InvalidParams, FString::Printf(TEXT("Missing params._meta[\"%s\"]"), ProtoBoyMCP::MetaClientCapabilities)) };
	}

	// Mirrored headers must be present and match the body.
	auto Mismatch = [&Id](const FString& What)
	{
		return FReply{ 400, ProtoBoyMCP::MakeRpcError(Id, ProtoBoyMCP::HeaderMismatch, FString::Printf(TEXT("Header mismatch: %s"), *What)) };
	};

	if (!ProtoBoyHttp::HasHeader(Request, TEXT("MCP-Protocol-Version")))
	{
		return Mismatch(TEXT("MCP-Protocol-Version header is missing"));
	}
	if (ProtoBoyHttp::GetHeader(Request, TEXT("MCP-Protocol-Version")) != Version)
	{
		return Mismatch(TEXT("MCP-Protocol-Version header does not match the body"));
	}
	if (!ProtoBoyHttp::HasHeader(Request, TEXT("Mcp-Method")))
	{
		return Mismatch(TEXT("Mcp-Method header is missing"));
	}
	if (!ProtoBoyHttp::GetHeader(Request, TEXT("Mcp-Method")).Equals(Method, ESearchCase::CaseSensitive))
	{
		return Mismatch(TEXT("Mcp-Method header does not match the body"));
	}
	if (Method == TEXT("tools/call") || Method == TEXT("prompts/get"))
	{
		if (!ProtoBoyHttp::HasHeader(Request, TEXT("Mcp-Name")))
		{
			return Mismatch(TEXT("Mcp-Name header is missing"));
		}
		FString BodyName;
		Params->TryGetStringField(TEXT("name"), BodyName);
		if (!ProtoBoyMCP::DecodeHeaderValue(ProtoBoyHttp::GetHeader(Request, TEXT("Mcp-Name"))).Equals(BodyName, ESearchCase::CaseSensitive))
		{
			return Mismatch(TEXT("Mcp-Name header does not match the body"));
		}
	}

	return {};
}

TSharedRef<FJsonObject> FProtoBoyMCPServer::MakeInitializeResult(const TSharedPtr<FJsonObject>& Params)
{
	// Answer with the client's version if we speak it, otherwise our preferred legacy version.
	FString Requested;
	Params->TryGetStringField(TEXT("protocolVersion"), Requested);
	const FString Version = ProtoBoyMCP::IsLegacyVersion(Requested) ? Requested : FString(ProtoBoyMCP::LegacyVersions[0]);

	const TSharedPtr<FJsonObject>* ClientInfo = nullptr;
	FString ClientName;
	if (!Params->TryGetObjectField(TEXT("clientInfo"), ClientInfo) || !(*ClientInfo)->TryGetStringField(TEXT("name"), ClientName))
	{
		ClientName = TEXT("Unknown client");
	}
	LastLegacyClient = ClientName;
	NoteClient(ClientName);

	TSharedRef<FJsonObject> ToolsCapability = MakeShared<FJsonObject>();
	ToolsCapability->SetBoolField(TEXT("listChanged"), false);
	TSharedRef<FJsonObject> PromptsCapability = MakeShared<FJsonObject>();
	PromptsCapability->SetBoolField(TEXT("listChanged"), false);
	TSharedRef<FJsonObject> Capabilities = MakeShared<FJsonObject>();
	Capabilities->SetObjectField(TEXT("tools"), ToolsCapability);
	Capabilities->SetObjectField(TEXT("prompts"), PromptsCapability);

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("protocolVersion"), Version);
	Result->SetObjectField(TEXT("capabilities"), Capabilities);
	Result->SetObjectField(TEXT("serverInfo"), ProtoBoyMCP::MakeServerInfo());
	Result->SetStringField(TEXT("instructions"), ProtoBoyMCP::Instructions);
	return Result;
}

TSharedRef<FJsonObject> FProtoBoyMCPServer::MakeDiscoverResult() const
{
	TSharedRef<FJsonObject> Capabilities = MakeShared<FJsonObject>();
	Capabilities->SetObjectField(TEXT("tools"), MakeShared<FJsonObject>());
	Capabilities->SetObjectField(TEXT("prompts"), MakeShared<FJsonObject>());

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("supportedVersions"), ProtoBoyMCP::AllSupportedVersions());
	Result->SetObjectField(TEXT("capabilities"), Capabilities);
	Result->SetStringField(TEXT("instructions"), ProtoBoyMCP::Instructions);
	return Result;
}

TSharedRef<FJsonObject> FProtoBoyMCPServer::MakeToolsListResult() const
{
	TArray<TSharedPtr<FJsonValue>> ToolArray;
	for (const FProtoBoyMCPTool& Tool : Tools)
	{
		if (!IsToolEnabled(Tool))
		{
			continue;
		}

		TSharedRef<FJsonObject> Annotations = MakeShared<FJsonObject>();
		Annotations->SetBoolField(TEXT("readOnlyHint"), !Tool.bWrites);
		Annotations->SetBoolField(TEXT("destructiveHint"), false);

		TSharedPtr<FJsonObject> Schema = Tool.InputSchema;
		if (!Schema.IsValid())
		{
			Schema = MakeShared<FJsonObject>();
			Schema->SetStringField(TEXT("type"), TEXT("object"));
			Schema->SetBoolField(TEXT("additionalProperties"), false);
		}

		TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"), Tool.Name.ToString());
		Entry->SetStringField(TEXT("title"), Tool.Title.ToString());
		Entry->SetStringField(TEXT("description"), Tool.Description.ToString());
		Entry->SetObjectField(TEXT("inputSchema"), Schema);
		Entry->SetObjectField(TEXT("annotations"), Annotations);
		ToolArray.Add(MakeShared<FJsonValueObject>(Entry));
	}

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("tools"), ToolArray);
	return Result;
}

TSharedRef<FJsonObject> FProtoBoyMCPServer::MakePromptsListResult() const
{
	TArray<TSharedPtr<FJsonValue>> PromptArray;
	for (const FProtoBoyMCPPrompt& Prompt : Prompts)
	{
		TArray<TSharedPtr<FJsonValue>> Args;
		for (const FProtoBoyMCPPrompt::FArgument& Arg : Prompt.Arguments)
		{
			TSharedRef<FJsonObject> ArgObject = MakeShared<FJsonObject>();
			ArgObject->SetStringField(TEXT("name"), Arg.Name);
			ArgObject->SetStringField(TEXT("description"), Arg.Description);
			ArgObject->SetBoolField(TEXT("required"), Arg.bRequired);
			Args.Add(MakeShared<FJsonValueObject>(ArgObject));
		}

		TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"), Prompt.Name);
		Entry->SetStringField(TEXT("title"), Prompt.Title);
		Entry->SetStringField(TEXT("description"), Prompt.Description);
		Entry->SetArrayField(TEXT("arguments"), Args);
		PromptArray.Add(MakeShared<FJsonValueObject>(Entry));
	}

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("prompts"), PromptArray);
	return Result;
}

TSharedPtr<FJsonObject> FProtoBoyMCPServer::MakePromptResult(const TSharedPtr<FJsonObject>& Params, FString& OutError) const
{
	FString Name;
	Params->TryGetStringField(TEXT("name"), Name);
	const FProtoBoyMCPPrompt* Prompt = Prompts.FindByPredicate([&Name](const FProtoBoyMCPPrompt& P) { return P.Name == Name; });
	if (!Prompt)
	{
		OutError = FString::Printf(TEXT("Unknown prompt: %s"), *Name);
		return nullptr;
	}

	TMap<FString, FString> Args;
	const TSharedPtr<FJsonObject>* ArgsObject = nullptr;
	if (Params->TryGetObjectField(TEXT("arguments"), ArgsObject))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*ArgsObject)->Values)
		{
			FString Value;
			if (Pair.Value.IsValid() && Pair.Value->TryGetString(Value))
			{
				Args.Add(Pair.Key, Value);
			}
		}
	}

	for (const FProtoBoyMCPPrompt::FArgument& Arg : Prompt->Arguments)
	{
		if (Arg.bRequired && Args.FindRef(Arg.Name).TrimStartAndEnd().IsEmpty())
		{
			OutError = FString::Printf(TEXT("Prompt %s needs the \"%s\" argument."), *Name, *Arg.Name);
			return nullptr;
		}
	}

	TSharedRef<FJsonObject> Content = MakeShared<FJsonObject>();
	Content->SetStringField(TEXT("type"), TEXT("text"));
	Content->SetStringField(TEXT("text"), Prompt->Build(Args));

	TSharedRef<FJsonObject> UserMessage = MakeShared<FJsonObject>();
	UserMessage->SetStringField(TEXT("role"), TEXT("user"));
	UserMessage->SetObjectField(TEXT("content"), Content);

	TArray<TSharedPtr<FJsonValue>> Messages;
	Messages.Add(MakeShared<FJsonValueObject>(UserMessage));

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("description"), Prompt->Description);
	Result->SetArrayField(TEXT("messages"), Messages);
	return Result;
}

bool FProtoBoyMCPServer::CallTool(const TSharedPtr<FJsonObject>& Params, const FString& ClientName, FProtoBoyMCPToolDone OnResult, FString& OutError)
{
	FString Name;
	Params->TryGetStringField(TEXT("name"), Name);
	const FProtoBoyMCPTool* Tool = Tools.FindByPredicate([&Name](const FProtoBoyMCPTool& T) { return T.Name.ToString() == Name; });
	if (!Tool || !Tool->Handler)
	{
		OutError = FString::Printf(TEXT("Unknown tool: %s"), *Name);
		return false;
	}
	if (!IsToolEnabled(*Tool))
	{
		OutError = Tool->bWrites && !GetDefault<UProtoBoySettings>()->bAllowMCPWriteTools
			? FString::Printf(TEXT("%s changes the project, and this project doesn't allow agent changes (ProtoBoy > MCP Settings > Server). Ask the user to allow them, or make the change yourself."), *Name)
			: Tool->IsAvailable && !Tool->IsAvailable()
				? FString::Printf(TEXT("%s isn't available: its feature is turned off for this project in ProtoBoy."), *Name)
				: FString::Printf(TEXT("%s is turned off in ProtoBoy's MCP settings."), *Name);
		return false;
	}

	FProtoBoyMCPToolCall Call;
	const TSharedPtr<FJsonObject>* ArgsObject = nullptr;
	Call.Args = Params->TryGetObjectField(TEXT("arguments"), ArgsObject) ? *ArgsObject : MakeShared<FJsonObject>();
	Call.ClientName = ClientName;

	// Copy what the async path needs; the Tools array could change while waiting for approval.
	const FProtoBoyMCPToolHandler Handler = Tool->Handler;
	const FText Title = Tool->Title;
	const bool bWrites = Tool->bWrites;
	const FString ArgsSummary = ProtoBoyMCP::SummarizeArgs(Call.Args);

	// Write tools: log the outcome.
	FProtoBoyMCPToolDone Done = [OnResult = MoveTemp(OnResult), bWrites, Name, ClientName, ArgsSummary](const FProtoBoyMCPToolResult& Result)
	{
		if (bWrites)
		{
			FProtoBoyAgentActivityLog::Append(ClientName, Name, ArgsSummary, Result.bIsError ? FString(TEXT("error: ")) + Result.Text.Left(300) : FString(TEXT("ok")));
		}
		OnResult(Result);
	};

	if (bWrites && GetDefault<UProtoBoySettings>()->bConfirmMCPWriteTools)
	{
		FProtoBoyMCPApprovals::Request(ClientName, Title, ArgsSummary,
			[Handler, Call, Done = MoveTemp(Done), Name, ClientName, ArgsSummary](bool bApproved) mutable
			{
				if (!bApproved)
				{
					FProtoBoyAgentActivityLog::Append(ClientName, Name, ArgsSummary, TEXT("denied by user"));
					Done(FProtoBoyMCPToolResult::Error(TEXT("The user denied this change in the Unreal Editor. Don't retry it; ask the user how to proceed.")));
					return;
				}
				Handler(Call, MoveTemp(Done));
			});
		return true;
	}

	Handler(Call, MoveTemp(Done));
	return true;
}

bool FProtoBoyMCPServer::IsAuthorized(const FHttpServerRequest& Request)
{
	const FString Header = ProtoBoyHttp::GetHeader(Request, TEXT("Authorization"));
	const FString Prefix = TEXT("Bearer ");
	if (!Header.StartsWith(Prefix, ESearchCase::IgnoreCase))
	{
		return false;
	}

	// Constant-time compare so the token can't be guessed byte by byte from response timing.
	const FString Presented = Header.Mid(Prefix.Len()).TrimStartAndEnd();
	const FString& Expected = GetAuthToken();
	if (Presented.Len() != Expected.Len())
	{
		return false;
	}
	uint32 Diff = 0;
	for (int32 i = 0; i < Expected.Len(); ++i)
	{
		Diff |= static_cast<uint32>(Presented[i] ^ Expected[i]);
	}
	return Diff == 0;
}

bool FProtoBoyMCPServer::IsAllowedOrigin(const FHttpServerRequest& Request)
{
	// Non-browser clients send no Origin. Browsers always do; only allow pages served from this machine.
	if (!ProtoBoyHttp::HasHeader(Request, TEXT("Origin")))
	{
		return true;
	}

	FString Origin = ProtoBoyHttp::GetHeader(Request, TEXT("Origin")).ToLower();
	if (!Origin.RemoveFromStart(TEXT("http://")) && !Origin.RemoveFromStart(TEXT("https://")))
	{
		return false;
	}

	FString Host = Origin;
	if (Host.StartsWith(TEXT("[")))
	{
		// IPv6 literal, e.g. [::1]:8080
		int32 Close = INDEX_NONE;
		Host.FindChar(TEXT(']'), Close);
		Host = Close != INDEX_NONE ? Host.Left(Close + 1) : Host;
	}
	else
	{
		int32 Colon = INDEX_NONE;
		if (Host.FindChar(TEXT(':'), Colon))
		{
			Host.LeftInline(Colon);
		}
	}
	return Host == TEXT("localhost") || Host == TEXT("127.0.0.1") || Host == TEXT("[::1]");
}

void FProtoBoyMCPServer::NoteClient(const FString& ClientName)
{
	const bool bNew = !GetRecentClients().Contains(ClientName);
	ClientLastSeen.Add(ClientName, FPlatformTime::Seconds());
	if (bNew)
	{
		UE_LOG(LogProtoBoy, Log, TEXT("MCP client connected: %s"), *ClientName);
		OnStatusChanged.Broadcast();
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Token and client setup

const FString& FProtoBoyMCPServer::GetAuthToken()
{
	if (Connection.AuthToken.IsEmpty())
	{
		RegenerateAuthToken();
	}
	return Connection.AuthToken;
}

void FProtoBoyMCPServer::RegenerateAuthToken()
{
	// Two GUIDs = 256 bits from the platform's GUID generator.
	Connection.AuthToken = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower()
		+ FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
	FProtoBoyMCPConnectionStore::Save(Connection);

	// Clients still holding the old token would be rejected, so re-write their entries.
	for (const EProtoBoyMCPClient Client : Connection.ConfiguredClients)
	{
		FText Error;
		if (!FProtoBoyMCPClientConfig::WriteConfig(Client, GetUrl(), Connection.AuthToken, Error))
		{
			UE_LOG(LogProtoBoy, Warning, TEXT("Couldn't update %s with the new MCP token: %s"),
				*FProtoBoyMCPClientConfig::GetClientName(Client).ToString(), *Error.ToString());
		}
	}

	OnStatusChanged.Broadcast();
}

bool FProtoBoyMCPServer::ConnectClient(EProtoBoyMCPClient Client, FText& OutError)
{
	if (!FProtoBoyMCPClientConfig::WriteConfig(Client, GetUrl(), GetAuthToken(), OutError))
	{
		return false;
	}

	Connection.ConfiguredClients.AddUnique(Client);
	FProtoBoyMCPConnectionStore::Save(Connection);

	// Not fatal: the agent still has the tools and server instructions without it.
	FText GuideError;
	if (!ProtoBoyAgentGuide::WriteForClient(Client, GuideError))
	{
		UE_LOG(LogProtoBoy, Warning, TEXT("Couldn't update the agent guide for %s: %s"),
			*FProtoBoyMCPClientConfig::GetClientName(Client).ToString(), *GuideError.ToString());
	}

	if (!bRunning)
	{
		Start(GetDefault<UProtoBoySettings>()->MCPPort);
	}

	OnStatusChanged.Broadcast();
	return true;
}

void FProtoBoyMCPServer::RefreshAgentGuides()
{
	for (const EProtoBoyMCPClient Client : Connection.ConfiguredClients)
	{
		FText Error;
		if (!ProtoBoyAgentGuide::WriteForClient(Client, Error))
		{
			UE_LOG(LogProtoBoy, Warning, TEXT("Couldn't update the agent guide for %s: %s"),
				*FProtoBoyMCPClientConfig::GetClientName(Client).ToString(), *Error.ToString());
		}
	}
}

bool FProtoBoyMCPServer::DisconnectClient(EProtoBoyMCPClient Client, FText& OutError)
{
	if (!FProtoBoyMCPClientConfig::RemoveConfig(Client, OutError))
	{
		return false;
	}

	Connection.ConfiguredClients.Remove(Client);
	FProtoBoyMCPConnectionStore::Save(Connection);
	OnStatusChanged.Broadcast();
	return true;
}

#undef LOCTEXT_NAMESPACE
