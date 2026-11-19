#pragma once

#include "Containers/Ticker.h"
#include "Daemon/PBDaemonClient.h"
#include "Docs/PBDocsService.h"
#include "Docs/PBDocMap.h"
#include "Docs/Obsidian/PBObsidianConnector.h"
#include "EditorSubsystem.h"
#include "Health/PBDocHealthService.h"
#include "MCP/PBMCPServer.h"
#include "Tasks/PBTaskBoard.h"
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
	FProtoBoyDocsService& GetDocsService() const { return *DocsService; }
	FProtoBoyDaemonClient& GetDaemonClient() const { return *DaemonClient; }
	FProtoBoyMCPServer& GetMCPServer() const { return *MCPServer; }
	FProtoBoyTrelloConnector& GetTrelloConnector() const { return *TrelloConnector; }
	FProtoBoyTrelloClient& GetTrelloClient() const { return *TrelloClient; }
	/** The docs, by layer (Game, Design, Tech, Content), read from their frontmatter. */
	FProtoBoyDocMap& GetDocMap() const { return *DocMap; }

	/** The docs platform in use (Obsidian when connected, else the project's DocsDirectory). */
	IProtoBoyDocsProvider& GetDocsProvider() const { return DocsService->GetProvider(); }

	/** The task board in use, or null if none is connected. Check IsReady before use. */
	IProtoBoyTaskProvider* GetTaskProvider() const;
	/** The board as ProtoBoy last saw it: category sync state and the loaded tasks (filled by Sync). */
	FProtoBoyTaskBoard& GetTaskBoard() const { return *TaskBoard; }
	/** The documentation health checks: the running check, the last report and the structure issues. */
	FProtoBoyDocHealthService& GetDocHealth() const { return *DocHealth; }

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
	TUniquePtr<FProtoBoyDocsService> DocsService;
	TUniquePtr<FProtoBoyDaemonClient> DaemonClient;
	TUniquePtr<FProtoBoyMCPServer> MCPServer;
	TUniquePtr<FProtoBoyTrelloConnector> TrelloConnector;
	TUniquePtr<FProtoBoyTrelloClient> TrelloClient;
	TUniquePtr<FProtoBoyDocMap> DocMap;
	TUniquePtr<FProtoBoyTaskBoard> TaskBoard;
	TUniquePtr<FProtoBoyDocHealthService> DocHealth;
	/** Re-read the doc map when the docs folder changes (Obsidian connect / disconnect). */
	FDelegateHandle DocsConnectionChangedHandle;
};
