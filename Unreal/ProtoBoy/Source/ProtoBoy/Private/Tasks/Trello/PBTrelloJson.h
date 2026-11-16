#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

/** JSON helpers shared by the Trello client's files (PBTrelloClient, PBTrelloBoardSetup, PBTrelloCategorySync). */
namespace ProtoBoyTrelloClient
{
	inline const TCHAR* const ApiRoot = TEXT("https://api.trello.com/1/");
	/** Card fields ProtoBoy reads (FProtoBoyTrelloClient::ParseCard). */
	inline const TCHAR* const CardFields = TEXT("fields=name,desc,idList,idBoard,shortLink,shortUrl,labels,due,dateLastActivity");

	inline FString ToJson(const TSharedPtr<FJsonObject>& Object)
	{
		FString Out;
		if (Object.IsValid())
		{
			FJsonSerializer::Serialize(Object.ToSharedRef(), TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out));
		}
		return Out;
	}

	inline FString GetString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		FString Value;
		if (Object.IsValid())
		{
			Object->TryGetStringField(Field, Value);
		}
		return Value;
	}
}
