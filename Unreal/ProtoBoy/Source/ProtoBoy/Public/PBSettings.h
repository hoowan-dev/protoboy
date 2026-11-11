#pragma once

#include "Engine/DeveloperSettings.h"
#include "PBSettings.generated.h"

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

	/**
	 * Team default for the folder inside the Obsidian vault (relative to the vault root); pre-fills the Connect window.
	 * Empty means "ProtoBoy/<ProjectName>". Each user's actual connection is saved in Saved/ProtoBoy/DocsConnection.json.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Docs|Obsidian")
	FString ObsidianDocsFolder;

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

	/** Start ProtoBoy's MCP server with the editor so agents (Claude, Gemini, ...) can call ProtoBoy tools. */
	UPROPERTY(Config, EditAnywhere, Category = "MCP")
	bool bEnableMCPServer = false;

	/** Local port for the MCP server. It only listens on 127.0.0.1. */
	UPROPERTY(Config, EditAnywhere, Category = "MCP", meta = (ClampMin = "1024", ClampMax = "65535"))
	int32 MCPPort = 47801;

	/** Let agents call tools that modify the project (write ADRs, retag assets). Read-only tools are always allowed. */
	UPROPERTY(Config, EditAnywhere, Category = "MCP")
	bool bAllowMCPWriteTools = false;

	/** Ask in the editor before running each write tool call. */
	UPROPERTY(Config, EditAnywhere, Category = "MCP", meta = (EditCondition = "bAllowMCPWriteTools"))
	bool bConfirmMCPWriteTools = true;

	/** MCP tools turned off for this project (by tool name). Edited from the MCP Settings window. */
	UPROPERTY(Config, EditAnywhere, Category = "MCP")
	TArray<FName> DisabledMCPTools;

	/** Connect to the ProtoBoy background daemon. */
	UPROPERTY(Config, EditAnywhere, Category = "Daemon")
	bool bConnectToDaemon = false;

	UPROPERTY(Config, EditAnywhere, Category = "Daemon", meta = (EditCondition = "bConnectToDaemon"))
	FString DaemonUrl = TEXT("ws://127.0.0.1:47800");
};
