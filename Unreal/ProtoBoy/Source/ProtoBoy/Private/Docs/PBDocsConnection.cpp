#include "Docs/PBDocsConnection.h"

#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PBModule.h"

FString FProtoBoyDocsConnectionStore::GetFilePath()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ProtoBoy/DocsConnection.json"));
}

bool FProtoBoyDocsConnectionStore::Load(FProtoBoyDocsConnection& Out)
{
	Out = FProtoBoyDocsConnection();

	const FString Path = GetFilePath();
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		return false;
	}

	FProtoBoyDocsConnection Loaded;
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Loaded))
	{
		UE_LOG(LogProtoBoy, Warning, TEXT("Could not parse %s; ignoring it."), *Path);
		return false;
	}

	if (Loaded.Version > FProtoBoyDocsConnection::CurrentVersion)
	{
		UE_LOG(LogProtoBoy, Warning, TEXT("%s is version %d (newer than %d); ignoring it."),
			*Path, Loaded.Version, FProtoBoyDocsConnection::CurrentVersion);
		return false;
	}
	// TODO: Migrate older versions here when CurrentVersion is bumped.

	Out = MoveTemp(Loaded);
	return true;
}

bool FProtoBoyDocsConnectionStore::Save(const FProtoBoyDocsConnection& Connection)
{
	FString Json;
	if (!FJsonObjectConverter::UStructToJsonObjectString(Connection, Json))
	{
		return false;
	}

	const FString Path = GetFilePath();
	if (!FFileHelper::SaveStringToFile(Json, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogProtoBoy, Error, TEXT("Could not write %s."), *Path);
		return false;
	}
	return true;
}

void FProtoBoyDocsConnectionStore::Clear()
{
	IFileManager::Get().Delete(*GetFilePath(), false, true, true);
}
