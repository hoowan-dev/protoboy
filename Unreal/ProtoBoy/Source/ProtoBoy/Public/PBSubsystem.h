#pragma once

#include "Assets/PBAssetLifecycle.h"
#include "Daemon/PBDaemonClient.h"
#include "Docs/PBDocGenerator.h"
#include "Docs/Obsidian/PBObsidianConnector.h"
#include "EditorSubsystem.h"
#include "Health/PBHealthChecker.h"
#include "MCP/PBMCPServer.h"
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
	FProtoBoyHealthChecker& GetHealthChecker() const { return *HealthChecker; }
	FProtoBoyAssetLifecycle& GetAssetLifecycle() const { return *AssetLifecycle; }
	FProtoBoyDaemonClient& GetDaemonClient() const { return *DaemonClient; }
	FProtoBoyMCPServer& GetMCPServer() const { return *MCPServer; }
	FProtoBoyTrelloConnector& GetTrelloConnector() const { return *TrelloConnector; }

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

	TUniquePtr<FProtoBoyObsidianConnector> ObsidianConnector;
	TUniquePtr<FProtoBoyDocGenerator> DocGenerator;
	TUniquePtr<FProtoBoyHealthChecker> HealthChecker;
	TUniquePtr<FProtoBoyAssetLifecycle> AssetLifecycle;
	TUniquePtr<FProtoBoyDaemonClient> DaemonClient;
	TUniquePtr<FProtoBoyMCPServer> MCPServer;
	TUniquePtr<FProtoBoyTrelloConnector> TrelloConnector;
};
