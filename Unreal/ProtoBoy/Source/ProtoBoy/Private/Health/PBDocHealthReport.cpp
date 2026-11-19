#include "Health/PBDocHealthReport.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PBModule.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace ProtoBoyDocHealthReport
{
	constexpr int32 CurrentVersion = 1;

	FString GetFilePath()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ProtoBoy/DocHealth/LastReport.json"));
	}

	bool Load(FProtoBoyDocHealthReport& Out)
	{
		Out = FProtoBoyDocHealthReport();

		const FString Path = GetFilePath();
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *Path))
		{
			return false;
		}
		TSharedPtr<FJsonObject> Root;
		int32 Version = 0;
		FString Checked;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid()
			|| !Root->TryGetNumberField(TEXT("version"), Version) || Version > CurrentVersion
			|| !Root->TryGetStringField(TEXT("checked"), Checked) || !FDateTime::ParseIso8601(*Checked, Out.CheckedUtc))
		{
			UE_LOG(LogProtoBoy, Warning, TEXT("Could not read %s; ignoring it."), *Path);
			return false;
		}
		Root->TryGetStringField(TEXT("agent"), Out.AgentName);
		Root->TryGetNumberField(TEXT("docsChecked"), Out.NumDocsChecked);
		Root->TryGetNumberField(TEXT("changedFiles"), Out.NumChangedFiles);
		Root->TryGetBoolField(TEXT("agentAnswered"), Out.bAgentAnswered);

		const TArray<TSharedPtr<FJsonValue>>* Docs = nullptr;
		if (Root->TryGetArrayField(TEXT("docs"), Docs))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Docs)
			{
				const TSharedPtr<FJsonObject>* Object = nullptr;
				if (!Value->TryGetObject(Object))
				{
					continue;
				}
				FProtoBoyStaleDoc Doc;
				FString Modified;
				(*Object)->TryGetStringField(TEXT("id"), Doc.DocId);
				(*Object)->TryGetStringField(TEXT("path"), Doc.Path);
				(*Object)->TryGetStringField(TEXT("title"), Doc.Title);
				(*Object)->TryGetStringField(TEXT("confidence"), Doc.Confidence);
				(*Object)->TryGetStringField(TEXT("reason"), Doc.Reason);
				(*Object)->TryGetStringArrayField(TEXT("files"), Doc.Files);
				if (!(*Object)->TryGetStringField(TEXT("docModified"), Modified) || !FDateTime::ParseIso8601(*Modified, Doc.DocModifiedUtc))
				{
					Doc.DocModifiedUtc = FDateTime::MinValue();
				}
				Out.Docs.Add(MoveTemp(Doc));
			}
		}
		return true;
	}

	bool Save(const FProtoBoyDocHealthReport& Report)
	{
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("version"), CurrentVersion);
		Root->SetStringField(TEXT("checked"), Report.CheckedUtc.ToIso8601());
		Root->SetStringField(TEXT("agent"), Report.AgentName);
		Root->SetNumberField(TEXT("docsChecked"), Report.NumDocsChecked);
		Root->SetNumberField(TEXT("changedFiles"), Report.NumChangedFiles);
		Root->SetBoolField(TEXT("agentAnswered"), Report.bAgentAnswered);

		TArray<TSharedPtr<FJsonValue>> Docs;
		for (const FProtoBoyStaleDoc& Doc : Report.Docs)
		{
			const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
			Object->SetStringField(TEXT("id"), Doc.DocId);
			Object->SetStringField(TEXT("path"), Doc.Path);
			Object->SetStringField(TEXT("title"), Doc.Title);
			Object->SetStringField(TEXT("confidence"), Doc.Confidence);
			Object->SetStringField(TEXT("reason"), Doc.Reason);
			Object->SetStringField(TEXT("docModified"), Doc.DocModifiedUtc.ToIso8601());
			TArray<TSharedPtr<FJsonValue>> Files;
			for (const FString& File : Doc.Files)
			{
				Files.Add(MakeShared<FJsonValueString>(File));
			}
			Object->SetArrayField(TEXT("files"), Files);
			Docs.Add(MakeShared<FJsonValueObject>(Object));
		}
		Root->SetArrayField(TEXT("docs"), Docs);

		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		const FString Path = GetFilePath();
		if (!FJsonSerializer::Serialize(Root, Writer) || !FFileHelper::SaveStringToFile(Json, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			UE_LOG(LogProtoBoy, Error, TEXT("Could not write %s."), *Path);
			return false;
		}
		return true;
	}
}
