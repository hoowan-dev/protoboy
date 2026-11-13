#pragma once

#include "Containers/Ticker.h"
#include "Daemon/PBDaemonClient.h"
#include "Docs/PBDocGenerator.h"
#include "Docs/PBDocMap.h"
#include "Docs/Obsidian/PBObsidianConnector.h"
#include "EditorSubsystem.h"
#include "MCP/PBMCPServer.h"
#include "Tasks/Trello/PBTrelloClient.h"
#include "Tasks/Trello/PBTrelloConnector.h"
#include "PBSubsystem.generated.h"

/**
 * Central hub for ProtoBoy in the editor. Owns one service per feature pillar
 * and (eventually) routes editor events into them.
 */
UCLASS()
class PROTOBOY_API UProtoBoySubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static UProtoBoySubsystem* Get();

	FProtoBoyObsidianConnector& GetObsidianConnector() const { return *ObsidianConnector; }
	FProtoBoyDocGenerator& GetDocGenerator() const { return *DocGenerator; }
	FProtoBoyDaemonClient& GetDaemonClient() const { return *DaemonClient; }
	FProtoBoyMCPServer& GetMCPServer() const { return *MCPServer; }
	FProtoBoyTrelloConnector& GetTrelloConnector() const { return *TrelloConnector; }
	FProtoBoyTrelloClient& GetTrelloClient() const { return *TrelloClient; }
	/** The docs, by layer (Game, Design, Tech, Content), read from their frontmatter. */
	FProtoBoyDocMap& GetDocMap() const { return *DocMap; }

	/** The docs platform in use (Obsidian when connected, else the project's DocsDirectory). */
	IProtoBoyDocsProvider& GetDocsProvider() const { return DocGenerator->GetProvider(); }

	/** The task board in use, or null if none is connected. Check IsReady before use. */
	IProtoBoyTaskProvider* GetTaskProvider() const;

private:
	// TODO: Bind asset registry / package save / live coding events and route them to the services.
	void BindEditorEvents();
	void UnbindEditorEvents();

	/**
	 * Slate's modal loop (any modal window, including ProtoBoy's settings windows) doesn't tick the core ticker,
	 * so our local HTTP listeners would stop answering. Pump them from the modal tick instead.
	 */
	void OnModalLoopTick(float DeltaTime);
	FDelegateHandle ModalLoopTickHandle;

	/** Runs queued HTTP callbacks (ProtoBoyHttp) outside modal loops. */
	bool OnCoreTick(float DeltaTime);
	FTSTicker::FDelegateHandle CoreTickHandle;

	TUniquePtr<FProtoBoyObsidianConnector> ObsidianConnector;
	TUniquePtr<FProtoBoyDocGenerator> DocGenerator;
	TUniquePtr<FProtoBoyDaemonClient> DaemonClient;
	TUniquePtr<FProtoBoyMCPServer> MCPServer;
	TUniquePtr<FProtoBoyTrelloConnector> TrelloConnector;
	TUniquePtr<FProtoBoyTrelloClient> TrelloClient;
	TUniquePtr<FProtoBoyDocMap> DocMap;
	/** Re-read the doc map when the docs folder changes (Obsidian connect / disconnect). */
	FDelegateHandle DocsConnectionChangedHandle;
};
