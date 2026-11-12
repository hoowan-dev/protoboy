#include "Sourcing/Fab/PBFabSource.h"

#include "GenericPlatform/GenericPlatformHttp.h"
#include "HAL/PlatformProcess.h"
#include "PBSettings.h"

#define LOCTEXT_NAMESPACE "ProtoBoyFab"

namespace ProtoBoyFab
{
	bool IsEnabled()
	{
		return GetDefault<UProtoBoySettings>()->bEnableFabSource;
	}

	FString BuildSearchUrl(const FSearch& Search)
	{
		FString Url = FString::Printf(TEXT("https://www.fab.com/search?q=%s"), *FGenericPlatformHttp::UrlEncode(Search.Query.TrimStartAndEnd()));
		if (!Search.ListingType.TrimStartAndEnd().IsEmpty())
		{
			Url += FString::Printf(TEXT("&listing_types=%s"), *FGenericPlatformHttp::UrlEncode(Search.ListingType.TrimStartAndEnd()));
		}
		const FString& Format = GetDefault<UProtoBoySettings>()->FabAssetFormat;
		if (!Format.TrimStartAndEnd().IsEmpty())
		{
			Url += FString::Printf(TEXT("&asset_formats=%s"), *FGenericPlatformHttp::UrlEncode(Format.TrimStartAndEnd()));
		}
		if (Search.bFreeOnly)
		{
			Url += TEXT("&is_free=1");
		}
		return Url;
	}

	bool OpenSearch(const FSearch& Search, FString& OutUrl, FText& OutError)
	{
		if (!IsEnabled())
		{
			OutError = LOCTEXT("FabDisabled", "Fab isn't enabled as an asset source for this project.");
			return false;
		}
		if (Search.Query.TrimStartAndEnd().IsEmpty())
		{
			OutError = LOCTEXT("NoQuery", "Enter what to search for.");
			return false;
		}

		OutUrl = BuildSearchUrl(Search);
		FString LaunchError;
		FPlatformProcess::LaunchURL(*OutUrl, nullptr, &LaunchError);
		if (!LaunchError.IsEmpty())
		{
			OutError = FText::Format(LOCTEXT("LaunchFailed", "Couldn't open the browser: {0}"), FText::FromString(LaunchError));
			return false;
		}
		return true;
	}
}

#undef LOCTEXT_NAMESPACE
