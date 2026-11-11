#pragma once

#include "Engine/DeveloperSettings.h"
#include "ProtoBoySettings.generated.h"

/** Project-wide ProtoBoy settings (Project Settings > Plugins > ProtoBoy). Saved to DefaultProtoBoy.ini. */
UCLASS(Config = ProtoBoy, DefaultConfig, meta = (DisplayName = "ProtoBoy"))
class PROTOBOY_API UProtoBoySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/** Folder (relative to the project directory) where living docs (GDD/TDD/ADRs) are written. */
	UPROPERTY(Config, EditAnywhere, Category = "Docs")
	FString DocsDirectory = TEXT("Docs/ProtoBoy");

	/** Generate TDD stubs automatically when new gameplay assets are created. */
	UPROPERTY(Config, EditAnywhere, Category = "Docs")
	bool bAutoGenerateTDDStubs = true;

	/** Tag newly imported assets as Prototype/Temp by default. */
	UPROPERTY(Config, EditAnywhere, Category = "Assets")
	bool bTagNewAssetsAsPrototype = true;

	/** Texture dimension above which the health check warns. */
	UPROPERTY(Config, EditAnywhere, Category = "Health", meta = (ClampMin = "1"))
	int32 MaxTextureSize = 4096;

	/** Static mesh triangle count above which the health check warns. */
	UPROPERTY(Config, EditAnywhere, Category = "Health", meta = (ClampMin = "1"))
	int32 MaxStaticMeshTriangles = 200000;

	/** Connect to the ProtoBoy background daemon. */
	UPROPERTY(Config, EditAnywhere, Category = "Daemon")
	bool bConnectToDaemon = false;

	UPROPERTY(Config, EditAnywhere, Category = "Daemon", meta = (EditCondition = "bConnectToDaemon"))
	FString DaemonUrl = TEXT("ws://127.0.0.1:47800");
};
