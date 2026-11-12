#include "MCP/PBAgentActivityLog.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PBModule.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FString FProtoBoyAgentActivityLog::GetFilePath()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ProtoBoy/AgentActivity.jsonl"));
}

void FProtoBoyAgentActivityLog::Append(const FString& ClientName, const FString& ToolName, const FString& ArgsSummary, const FString& Outcome)
{
	TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
	Entry->SetStringField(TEXT("time"), FDateTime::UtcNow().ToIso8601());
	Entry->SetStringField(TEXT("client"), ClientName);
	Entry->SetStringField(TEXT("tool"), ToolName);
	Entry->SetStringField(TEXT("args"), ArgsSummary);
	Entry->SetStringField(TEXT("outcome"), Outcome);

	FString Line;
	FJsonSerializer::Serialize(Entry, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Line));
	Line += TEXT("\n");

	const FString Path = GetFilePath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	FFileHelper::SaveStringToFile(Line, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);

	UE_LOG(LogProtoBoy, Log, TEXT("Agent %s ran %s: %s"), *ClientName, *ToolName, *Outcome);
}
