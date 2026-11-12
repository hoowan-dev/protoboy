#include "Docs/Obsidian/PBObsidianConnector.h"

#include "GenericPlatform/GenericPlatformHttp.h"
#include "HAL/PlatformProcess.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PBModule.h"
#include "PBSettings.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "ProtoBoyObsidian"

const TCHAR* FProtoBoyObsidianConnector::MarkerNoteName = TEXT("_ProtoBoy.md");

namespace ProtoBoyObsidian
{
	static const TCHAR* AutoBegin = TEXT("<!-- protoboy:auto:begin -->");
	static const TCHAR* AutoEnd = TEXT("<!-- protoboy:auto:end -->");

	static FString NormalizeDir(FString Path)
	{
		Path.TrimStartAndEndInline();
		FPaths::NormalizeDirectoryName(Path);
		return Path;
	}
}

TArray<FProtoBoyObsidianVault> FProtoBoyObsidianConnector::FindKnownVaults()
{
	TArray<FProtoBoyObsidianVault> Vaults;

	// Windows: %APPDATA%\obsidian\obsidian.json.
	// TODO: macOS (~/Library/Application Support/obsidian) and Linux (~/.config/obsidian).
	const FString AppData = FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA"));
	if (AppData.IsEmpty())
	{
		return Vaults;
	}

	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *(AppData / TEXT("obsidian/obsidian.json"))))
	{
		return Vaults;
	}

	// Format: { "vaults": { "<id>": { "path": "C:\\...", "ts": 123, "open": true }, ... } }
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
	{
		UE_LOG(LogProtoBoy, Warning, TEXT("Could not parse Obsidian's obsidian.json."));
		return Vaults;
	}

	const TSharedPtr<FJsonObject>* VaultMap = nullptr;
	if (!Root->TryGetObjectField(TEXT("vaults"), VaultMap))
	{
		return Vaults;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : (*VaultMap)->Values)
	{
		const TSharedPtr<FJsonObject> VaultObj = Entry.Value.IsValid() ? Entry.Value->AsObject() : nullptr;
		FString Path;
		if (!VaultObj.IsValid() || !VaultObj->TryGetStringField(TEXT("path"), Path))
		{
			continue;
		}

		Path = ProtoBoyObsidian::NormalizeDir(Path);
		if (IFileManager::Get().DirectoryExists(*Path))
		{
			Vaults.Add({ FPaths::GetCleanFilename(Path), Path });
		}
	}

	Vaults.Sort([](const FProtoBoyObsidianVault& A, const FProtoBoyObsidianVault& B) { return A.Name < B.Name; });
	return Vaults;
}

bool FProtoBoyObsidianConnector::ValidateVault(const FString& InVaultPath, FText& OutError, bool bCheckWritable)
{
	const FString VaultPath = ProtoBoyObsidian::NormalizeDir(InVaultPath);

	if (VaultPath.IsEmpty())
	{
		OutError = LOCTEXT("NoVault", "Choose a vault.");
		return false;
	}
	if (FPaths::IsRelative(VaultPath))
	{
		OutError = LOCTEXT("VaultRelative", "Vault path must be absolute.");
		return false;
	}
	if (!IFileManager::Get().DirectoryExists(*VaultPath))
	{
		OutError = LOCTEXT("VaultMissing", "Folder does not exist.");
		return false;
	}
	if (!IFileManager::Get().DirectoryExists(*(VaultPath / TEXT(".obsidian"))))
	{
		OutError = LOCTEXT("NotAVault", "Not an Obsidian vault (no .obsidian folder). Open it in Obsidian once first.");
		return false;
	}

	if (!bCheckWritable)
	{
		return true;
	}

	// Writability: create and remove a probe file.
	const FString Probe = VaultPath / TEXT(".protoboy-write-test");
	if (!FFileHelper::SaveStringToFile(TEXT("protoboy"), *Probe))
	{
		OutError = LOCTEXT("VaultReadOnly", "Vault folder is not writable.");
		return false;
	}
	IFileManager::Get().Delete(*Probe, false, true, true);

	return true;
}

bool FProtoBoyObsidianConnector::ValidateDocsFolder(const FString& InDocsFolder, FText& OutError)
{
	FString DocsFolder = InDocsFolder.TrimStartAndEnd();
	FPaths::NormalizeFilename(DocsFolder);

	if (DocsFolder.IsEmpty())
	{
		OutError = LOCTEXT("NoFolder", "Enter a folder inside the vault.");
		return false;
	}
	if (!FPaths::IsRelative(DocsFolder) || DocsFolder.StartsWith(TEXT("/")))
	{
		OutError = LOCTEXT("FolderAbsolute", "Folder must be relative to the vault root.");
		return false;
	}

	TArray<FString> Parts;
	DocsFolder.ParseIntoArray(Parts, TEXT("/"), true);
	for (const FString& Part : Parts)
	{
		if (Part == TEXT("..") || Part == TEXT("."))
		{
			OutError = LOCTEXT("FolderDots", "Folder can't contain \".\" or \"..\".");
			return false;
		}
		if (Part == TEXT(".obsidian"))
		{
			OutError = LOCTEXT("FolderObsidian", "Folder can't be inside .obsidian.");
			return false;
		}
	}

	FText Reason;
	if (!FPaths::ValidatePath(DocsFolder, &Reason))
	{
		OutError = Reason;
		return false;
	}
	return true;
}

FString FProtoBoyObsidianConnector::GetDefaultDocsFolder()
{
	return FString(TEXT("ProtoBoy")) / FApp::GetProjectName();
}

FString FProtoBoyObsidianConnector::MakeDocsRoot(const FString& VaultPath, const FString& DocsFolder)
{
	FString Folder = DocsFolder.TrimStartAndEnd();
	FPaths::NormalizeFilename(Folder);
	return ProtoBoyObsidian::NormalizeDir(VaultPath / Folder);
}

FProtoBoyObsidianConnector::FProtoBoyObsidianConnector()
{
	FProtoBoyDocsConnectionStore::Load(Connection);
	if (Connection.Platform != EProtoBoyDocPlatform::Obsidian)
	{
		return;
	}

	bVaultAvailable = IFileManager::Get().DirectoryExists(*(Connection.VaultPath / TEXT(".obsidian")));
	if (!bVaultAvailable)
	{
		// Keep the saved file so the connection comes back if the drive/folder reappears.
		UE_LOG(LogProtoBoy, Warning, TEXT("Saved Obsidian vault not found: %s"), *Connection.VaultPath);
	}
}

bool FProtoBoyObsidianConnector::Connect(const FString& InVaultPath, const FString& InDocsFolder, FText& OutError)
{
	const FString VaultPath = ProtoBoyObsidian::NormalizeDir(InVaultPath);
	FString DocsFolder = InDocsFolder.TrimStartAndEnd();
	FPaths::NormalizeFilename(DocsFolder);

	if (!ValidateVault(VaultPath, OutError, /*bCheckWritable*/ true) || !ValidateDocsFolder(DocsFolder, OutError))
	{
		return false;
	}

	const FString DocsRoot = MakeDocsRoot(VaultPath, DocsFolder);
	if (!IFileManager::Get().MakeDirectory(*DocsRoot, true))
	{
		OutError = FText::Format(LOCTEXT("MkdirFailed", "Could not create {0}."), FText::FromString(DocsRoot));
		return false;
	}

	if (!WriteMarkerNote(DocsRoot, OutError))
	{
		return false;
	}

	FProtoBoyDocsConnection NewConnection;
	NewConnection.Platform = EProtoBoyDocPlatform::Obsidian;
	NewConnection.VaultPath = VaultPath;
	NewConnection.DocsFolder = DocsFolder;
	NewConnection.ConnectedAtUtc = FDateTime::UtcNow().ToIso8601();

	if (!FProtoBoyDocsConnectionStore::Save(NewConnection))
	{
		OutError = FText::Format(LOCTEXT("SaveFailed", "Could not save {0}."),
			FText::FromString(FProtoBoyDocsConnectionStore::GetFilePath()));
		return false;
	}

	Connection = MoveTemp(NewConnection);
	bVaultAvailable = true;

	UE_LOG(LogProtoBoy, Log, TEXT("Connected to Obsidian vault '%s' (docs: %s)."), *GetVaultName(), *DocsRoot);
	OnConnectionChanged.Broadcast();
	return true;
}

void FProtoBoyObsidianConnector::Disconnect()
{
	UE_LOG(LogProtoBoy, Log, TEXT("Disconnected from Obsidian vault '%s'."), *GetVaultName());

	FProtoBoyDocsConnectionStore::Clear();
	Connection = FProtoBoyDocsConnection();
	bVaultAvailable = false;

	OnConnectionChanged.Broadcast();
}

bool FProtoBoyObsidianConnector::IsConnected() const
{
	return Connection.Platform == EProtoBoyDocPlatform::Obsidian && bVaultAvailable;
}

FString FProtoBoyObsidianConnector::GetVaultPath() const
{
	return Connection.VaultPath;
}

FString FProtoBoyObsidianConnector::GetVaultName() const
{
	return FPaths::GetCleanFilename(GetVaultPath());
}

FString FProtoBoyObsidianConnector::GetDocsFolder() const
{
	if (!Connection.DocsFolder.IsEmpty())
	{
		return Connection.DocsFolder;
	}
	const FString& ProjectDefault = GetDefault<UProtoBoySettings>()->ObsidianDocsFolder;
	return ProjectDefault.IsEmpty() ? GetDefaultDocsFolder() : ProjectDefault;
}

FString FProtoBoyObsidianConnector::GetDocsRoot() const
{
	return MakeDocsRoot(GetVaultPath(), GetDocsFolder());
}

bool FProtoBoyObsidianConnector::WriteMarkerNote(const FString& DocsRoot, FText& OutError)
{
	using namespace ProtoBoyObsidian;

	const FString NotePath = DocsRoot / MarkerNoteName;
	const FString Project = FApp::GetProjectName();
	const FString Engine = FString::Printf(TEXT("Unreal Engine %s"), *FEngineVersion::Current().ToString(EVersionComponent::Minor));

	const FString AutoBlock = FString::Printf(
		TEXT("%s\n")
		TEXT("Docs for **%s** are generated here by ProtoBoy.\n\n")
		TEXT("- Engine: %s\n")
		TEXT("- Last connected: %s\n")
		TEXT("%s"),
		AutoBegin, *Project, *Engine, *FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M")), AutoEnd);

	FString Existing;
	FString Content;
	if (FFileHelper::LoadFileToString(Existing, *NotePath))
	{
		// Refresh only the auto-generated block so human edits survive.
		const int32 Begin = Existing.Find(AutoBegin);
		const int32 End = Existing.Find(AutoEnd, ESearchCase::CaseSensitive, ESearchDir::FromStart, Begin);
		if (Begin == INDEX_NONE || End == INDEX_NONE)
		{
			// Markers were removed by hand; leave the note alone.
			return true;
		}
		Content = Existing.Left(Begin) + AutoBlock + Existing.Mid(End + FCString::Strlen(AutoEnd));
		if (Content == Existing)
		{
			return true;
		}
	}
	else
	{
		Content = FString::Printf(
			TEXT("---\n")
			TEXT("protoboy: true\n")
			TEXT("project: \"%s\"\n")
			TEXT("engine: \"%s\"\n")
			TEXT("---\n\n")
			TEXT("# ProtoBoy: %s\n\n")
			TEXT("%s\n\n")
			TEXT("## Notes\n\n")
			TEXT("Anything outside the auto-generated block above is yours; ProtoBoy won't overwrite it.\n"),
			*Project, *Engine, *Project, *AutoBlock);
	}

	if (!FFileHelper::SaveStringToFile(Content, *NotePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FText::Format(LOCTEXT("WriteFailed", "Could not write {0}."), FText::FromString(NotePath));
		return false;
	}
	return true;
}

bool FProtoBoyObsidianConnector::OpenInObsidian(const FString& DocsRelativePath, FText& OutError) const
{
	if (!IsConnected())
	{
		OutError = LOCTEXT("OpenNotConnected", "Obsidian isn't connected.");
		return false;
	}

	// obsidian://open?vault=<vault name>&file=<path inside the vault>
	const FString File = GetDocsFolder() / DocsRelativePath;
	const FString Uri = FString::Printf(TEXT("obsidian://open?vault=%s&file=%s"),
		*FGenericPlatformHttp::UrlEncode(GetVaultName()), *FGenericPlatformHttp::UrlEncode(File));

	FString LaunchError;
	FPlatformProcess::LaunchURL(*Uri, nullptr, &LaunchError);
	if (!LaunchError.IsEmpty())
	{
		OutError = FText::Format(LOCTEXT("OpenFailed", "Couldn't open Obsidian ({0}). Is it installed?"), FText::FromString(LaunchError));
		return false;
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
