#pragma once

#include "CoreMinimal.h"
#include "HttpServerRequest.h"

/**
 * Small helpers shared by ProtoBoy's local HTTP endpoints (MCP server, Trello callback).
 *
 * Note for helpers in .cpp files: the editor build is a unity build, so several .cpp files share one translation unit.
 * Don't put `using namespace` at file scope, and don't duplicate helper names across files; put shared ones here.
 */
namespace ProtoBoyHttp
{
	/** First value of a header, trimmed. Header names are case-insensitive (FString map keys compare that way). */
	inline FString GetHeader(const FHttpServerRequest& Request, const TCHAR* Name)
	{
		const TArray<FString>* Values = Request.Headers.Find(Name);
		return Values && Values->Num() > 0 ? (*Values)[0].TrimStartAndEnd() : FString();
	}

	inline bool HasHeader(const FHttpServerRequest& Request, const TCHAR* Name)
	{
		const TArray<FString>* Values = Request.Headers.Find(Name);
		return Values && Values->Num() > 0;
	}

	/** The request body decoded from UTF-8. */
	inline FString GetBodyAsString(const FHttpServerRequest& Request)
	{
		if (Request.Body.Num() == 0)
		{
			return FString();
		}
		const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Request.Body.GetData()), Request.Body.Num());
		return FString(Converted.Length(), Converted.Get());
	}
}
