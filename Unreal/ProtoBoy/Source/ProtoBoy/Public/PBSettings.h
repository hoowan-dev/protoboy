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

	/**
	 * The Architecture doc (end goal, milestone, systems, rules) that agents read before placing work. Path relative to the
	 * docs folder, e.g. "Architecture.md" or "Overview/Game Architecture.md". Set in Connect to Documentation.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Docs")
	FString ArchitectureDocPath = TEXT("Architecture.md");

	/** Generate TDD stubs automatically when new gameplay assets are created. */
	UPROPERTY(Config, EditAnywhere, Category = "Docs")
	bool bAutoGenerateTDDStubs = true;

	/** Start ProtoBoy's MCP server with the editor so agents (Claude, Gemini, ...) can call ProtoBoy tools. */
	UPROPERTY(Config, EditAnywhere, Category = "MCP")
	bool bEnableMCPServer = false;

	/** Local port for the MCP server. It only listens on 127.0.0.1. */
	UPROPERTY(Config, EditAnywhere, Category = "MCP", meta = (ClampMin = "1024", ClampMax = "65535"))
	int32 MCPPort = 47801;

	/** Let agents call tools that change docs or tasks (create docs, append sections, write ADRs, create/move/comment tasks). Read-only tools are always allowed. */
	UPROPERTY(Config, EditAnywhere, Category = "MCP")
	bool bAllowMCPWriteTools = false;

	/** Ask in the editor (a notification with Allow / Deny) before running each write tool call. */
	UPROPERTY(Config, EditAnywhere, Category = "MCP", meta = (EditCondition = "bAllowMCPWriteTools"))
	bool bConfirmMCPWriteTools = true;

	/** MCP tools turned off for this project (by tool name). Edited from the MCP Settings window. */
	UPROPERTY(Config, EditAnywhere, Category = "MCP")
	TArray<FName> DisabledMCPTools;

	/**
	 * Trello API key used for "Connect" (from a Power-Up at trello.com/power-ups/admin > API key).
	 * Empty uses ProtoBoy's built-in key. Not a secret, but never put the Power-Up's secret here.
	 * The Power-Up must list http://127.0.0.1:<TasksCallbackPort> under allowed origins.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Tasks|Trello")
	FString TrelloApiKey;

	/** The team's Trello board. Agents can only see and change cards on this board. Picked in Connect Task Board > Trello. */
	UPROPERTY(Config, EditAnywhere, Category = "Tasks|Trello")
	FString TrelloBoardId;

	/** Display name of TrelloBoardId (set by the board picker). */
	UPROPERTY(Config, EditAnywhere, Category = "Tasks|Trello")
	FString TrelloBoardName;

	/** Trello list that means "To Do". */
	UPROPERTY(Config, EditAnywhere, Category = "Tasks|Trello")
	FString TrelloToDoListId;

	/** Trello list that means "In Progress". */
	UPROPERTY(Config, EditAnywhere, Category = "Tasks|Trello")
	FString TrelloInProgressListId;

	/** Trello list that means "Done". */
	UPROPERTY(Config, EditAnywhere, Category = "Tasks|Trello")
	FString TrelloDoneListId;

	/** Local port Trello redirects back to while connecting. Must match the allowed origin on the Trello API key. */
	UPROPERTY(Config, EditAnywhere, Category = "Tasks", meta = (ClampMin = "1024", ClampMax = "65535"))
	int32 TasksCallbackPort = 47802;

	/** Connect to the ProtoBoy background daemon. */
	UPROPERTY(Config, EditAnywhere, Category = "Daemon")
	bool bConnectToDaemon = false;

	UPROPERTY(Config, EditAnywhere, Category = "Daemon", meta = (EditCondition = "bConnectToDaemon"))
	FString DaemonUrl = TEXT("ws://127.0.0.1:47800");
};
