#pragma once

#include "CoreMinimal.h"
#include "Docs/PBDocsConnection.h"

/** A vault Obsidian knows about (from its own obsidian.json). */
struct FProtoBoyObsidianVault
{
	FString Name; // Folder name, which is what Obsidian shows as the vault name.
	FString Path; // Absolute path, '/' separators.
};

/**
 * Connects ProtoBoy docs to an Obsidian vault through the file system.
 * A vault is a folder with a ".obsidian" subfolder; Obsidian picks up any Markdown written there.
 * "Connected" means a validated, writable docs folder inside the vault has been claimed and saved
 * to FProtoBoyDocsConnectionStore (per user).
 */
class PROTOBOY_API FProtoBoyObsidianConnector
{
public:
	/** Loads the saved connection, if any. */
	FProtoBoyObsidianConnector();

	/** Fired after Connect or Disconnect changes the connection. */
	FSimpleMulticastDelegate OnConnectionChanged;

	/** Name of the marker note written into the docs folder on connect. */
	static const TCHAR* MarkerNoteName;

	/** Vaults listed in %APPDATA%/obsidian/obsidian.json that still exist on disk. Empty if Obsidian isn't installed. */
	static TArray<FProtoBoyObsidianVault> FindKnownVaults();

	/**
	 * True if VaultPath is an existing folder containing ".obsidian".
	 * bCheckWritable also writes and deletes a probe file; skip it for as-you-type validation.
	 */
	static bool ValidateVault(const FString& VaultPath, FText& OutError, bool bCheckWritable = false);

	/** True if DocsFolder is a safe path relative to the vault root (no drive, no "..", valid characters). */
	static bool ValidateDocsFolder(const FString& DocsFolder, FText& OutError);

	/** Default folder inside the vault: "ProtoBoy/<ProjectName>". */
	static FString GetDefaultDocsFolder();

	/** Absolute docs folder for a vault + relative folder (no validation). */
	static FString MakeDocsRoot(const FString& VaultPath, const FString& DocsFolder);

	/** Validate, create the docs folder, write/refresh the marker note and save the connection. */
	bool Connect(const FString& VaultPath, const FString& DocsFolder, FText& OutError);

	/** Forget the connection (deletes the saved connection file). Leaves files in the vault untouched. */
	void Disconnect();

	/** A saved Obsidian connection exists and its vault was found when it was loaded or connected. */
	bool IsConnected() const;

	FString GetVaultPath() const;
	FString GetVaultName() const;

	/** Connected docs folder; else the project default (UProtoBoySettings::ObsidianDocsFolder); else "ProtoBoy/<Project>". */
	FString GetDocsFolder() const;

	/** Absolute docs root inside the vault. Only meaningful when IsConnected(). */
	FString GetDocsRoot() const;

	/** Open a doc (path relative to the docs root) in the Obsidian app via an obsidian://open link. */
	bool OpenInObsidian(const FString& DocsRelativePath, FText& OutError) const;

	const FProtoBoyDocsConnection& GetConnection() const { return Connection; }

private:
	static bool WriteMarkerNote(const FString& DocsRoot, FText& OutError);

	FProtoBoyDocsConnection Connection;

	/** Whether Connection's vault existed when last checked (load/connect). Cached so IsConnected() is cheap for UI. */
	bool bVaultAvailable = false;
};
