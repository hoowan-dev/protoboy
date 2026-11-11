#pragma once

#include "Assets/PBAssetLifecycle.h"
#include "Daemon/PBDaemonClient.h"
#include "Docs/PBDocGenerator.h"
#include "Docs/Obsidian/PBObsidianConnector.h"
#include "EditorSubsystem.h"
#include "Health/PBHealthChecker.h"
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

private:
	// TODO: Bind asset registry / package save / live coding events and route them to the services.
	void BindEditorEvents();
	void UnbindEditorEvents();

	TUniquePtr<FProtoBoyObsidianConnector> ObsidianConnector;
	TUniquePtr<FProtoBoyDocGenerator> DocGenerator;
	TUniquePtr<FProtoBoyHealthChecker> HealthChecker;
	TUniquePtr<FProtoBoyAssetLifecycle> AssetLifecycle;
	TUniquePtr<FProtoBoyDaemonClient> DaemonClient;
};
