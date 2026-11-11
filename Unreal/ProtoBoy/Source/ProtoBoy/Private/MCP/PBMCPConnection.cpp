#include "MCP/PBMCPConnection.h"

#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PBModule.h"

FString FProtoBoyMCPConnectionStore::GetFilePath()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ProtoBoy/MCPConnection.json"));
}

bool FProtoBoyMCPConnectionStore::Load(FProtoBoyMCPConnection& Out)
{
	Out = FProtoBoyMCPConnection();

	const FString Path = GetFilePath();
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		return false;
	}

	FProtoBoyMCPConnection Loaded;
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Loaded))
	{
		UE_LOG(LogProtoBoy, Warning, TEXT("Could not parse %s; ignoring it."), *Path);
		return false;
	}

	if (Loaded.Version > FProtoBoyMCPConnection::CurrentVersion)
	{
		UE_LOG(LogProtoBoy, Warning, TEXT("%s is version %d (newer than %d); ignoring it."),
			*Path, Loaded.Version, FProtoBoyMCPConnection::CurrentVersion);
		return false;
	}

	Out = MoveTemp(Loaded);
	return true;
}

bool FProtoBoyMCPConnectionStore::Save(const FProtoBoyMCPConnection& Connection)
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
