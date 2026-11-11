#pragma once

#include "Assets/ProtoBoyAssetLifecycle.h"
#include "Daemon/ProtoBoyDaemonClient.h"
#include "Docs/ProtoBoyDocGenerator.h"
#include "EditorSubsystem.h"
#include "Health/ProtoBoyHealthChecker.h"
#include "ProtoBoySubsystem.generated.h"

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

	FProtoBoyDocGenerator& GetDocGenerator() const { return *DocGenerator; }
	FProtoBoyHealthChecker& GetHealthChecker() const { return *HealthChecker; }
	FProtoBoyAssetLifecycle& GetAssetLifecycle() const { return *AssetLifecycle; }
	FProtoBoyDaemonClient& GetDaemonClient() const { return *DaemonClient; }

private:
	// TODO: Bind asset registry / package save / live coding events and route them to the services.
	void BindEditorEvents();
	void UnbindEditorEvents();

	TUniquePtr<FProtoBoyDocGenerator> DocGenerator;
	TUniquePtr<FProtoBoyHealthChecker> HealthChecker;
	TUniquePtr<FProtoBoyAssetLifecycle> AssetLifecycle;
	TUniquePtr<FProtoBoyDaemonClient> DaemonClient;
};
