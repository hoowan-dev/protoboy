#include "MCP/PBMCPServer.h"

#include "Misc/Guid.h"
#include "PBModule.h"
#include "PBSettings.h"

#define LOCTEXT_NAMESPACE "ProtoBoyMCPServer"

FProtoBoyMCPServer::FProtoBoyMCPServer()
{
	FProtoBoyMCPConnectionStore::Load(Connection);
	RegisterTools();
}

void FProtoBoyMCPServer::RegisterTools()
{
	// TODO: Give each tool a schema + handler that calls into the pillar services via UProtoBoySubsystem.
	Tools = {
		{ "get_project_health", LOCTEXT("ToolHealth", "Run the health check and return issues."), false },
		{ "get_viability_index", LOCTEXT("ToolPVI", "Return the Prototype-to-Production Viability Index."), false },
		{ "list_assets_by_lifecycle", LOCTEXT("ToolListAssets", "List assets tagged Untracked, Prototype or Production."), false },
		{ "get_greybox_matrix", LOCTEXT("ToolGreybox", "Return the Greybox Replacement Matrix."), false },
		{ "read_docs", LOCTEXT("ToolReadDocs", "Read the project's living docs (TDD stubs, ADRs)."), false },
		{ "write_adr", LOCTEXT("ToolWriteADR", "Create a new Architecture Decision Record."), true },
		{ "set_asset_lifecycle", LOCTEXT("ToolSetLifecycle", "Change an asset's lifecycle tag."), true },
	};
}

bool FProtoBoyMCPServer::Start(int32 InPort)
{
	if (bRunning)
	{
		return true;
	}

	// TODO: FHttpServerModule::Get().GetHttpRouter(InPort), bind POST/GET /mcp, reject requests whose
	//       Authorization header doesn't match GetAuthToken() or whose Origin isn't local, StartAllListeners().
	//       Handle initialize / tools/list / tools/call, filtering tools with IsToolEnabled().
	//       Calls that touch UObjects must run on the game thread.
	UE_LOG(LogProtoBoy, Warning, TEXT("MCP server is not implemented yet (requested port %d)."), InPort);
	return false;
}

void FProtoBoyMCPServer::Stop()
{
	if (!bRunning)
	{
		return;
	}

	// TODO: Unbind routes and stop listeners.
	bRunning = false;
	Port = 0;
	ConnectedClientCount = 0;
	OnStatusChanged.Broadcast();
}

FString FProtoBoyMCPServer::GetUrl() const
{
	const int32 UrlPort = bRunning ? Port : GetDefault<UProtoBoySettings>()->MCPPort;
	return FString::Printf(TEXT("http://127.0.0.1:%d/mcp"), UrlPort);
}

bool FProtoBoyMCPServer::IsToolEnabled(const FProtoBoyMCPTool& Tool) const
{
	const UProtoBoySettings* Settings = GetDefault<UProtoBoySettings>();
	if (Tool.bWrites && !Settings->bAllowMCPWriteTools)
	{
		return false;
	}
	return !Settings->DisabledMCPTools.Contains(Tool.Name);
}

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

	// TODO: Clients already configured with the old token stop working; re-write their configs.
	OnStatusChanged.Broadcast();
}

#undef LOCTEXT_NAMESPACE
