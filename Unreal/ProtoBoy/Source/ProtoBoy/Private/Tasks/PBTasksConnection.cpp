#include "Tasks/PBTasksConnection.h"

#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PBModule.h"

FString FProtoBoyTasksConnectionStore::GetFilePath()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ProtoBoy/TasksConnection.json"));
}

bool FProtoBoyTasksConnectionStore::Load(FProtoBoyTasksConnection& Out)
{
	Out = FProtoBoyTasksConnection();

	const FString Path = GetFilePath();
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		return false;
	}

	FProtoBoyTasksConnection Loaded;
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Loaded))
	{
		UE_LOG(LogProtoBoy, Warning, TEXT("Could not parse %s; ignoring it."), *Path);
		return false;
	}

	if (Loaded.Version > FProtoBoyTasksConnection::CurrentVersion)
	{
		UE_LOG(LogProtoBoy, Warning, TEXT("%s is version %d (newer than %d); ignoring it."),
			*Path, Loaded.Version, FProtoBoyTasksConnection::CurrentVersion);
		return false;
	}

	Out = MoveTemp(Loaded);
	return true;
}

bool FProtoBoyTasksConnectionStore::Save(const FProtoBoyTasksConnection& Connection)
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

void FProtoBoyTasksConnectionStore::Clear()
{
	IFileManager::Get().Delete(*GetFilePath(), false, true, true);
}
