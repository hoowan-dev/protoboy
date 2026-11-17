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

	FString GetListingType(EProtoBoyAssetType Type)
	{
		// Checked against fab.com search pages (each slug shows as "Results in <type>"). Unknown slugs are ignored by Fab.
		switch (Type)
		{
		case EProtoBoyAssetType::Model3D:         return TEXT("3d-model");
		case EProtoBoyAssetType::MaterialTexture: return TEXT("material");
		case EProtoBoyAssetType::SpriteFlipbook:  return TEXT("sprites-and-flipbooks");
		case EProtoBoyAssetType::Decal:           return TEXT("decal");
		case EProtoBoyAssetType::Brush:           return TEXT("brush");
		case EProtoBoyAssetType::HDRI:            return TEXT("hdri");
		case EProtoBoyAssetType::Animation:       return TEXT("animation");
		case EProtoBoyAssetType::Audio:           return TEXT("audio");
		case EProtoBoyAssetType::VFX:             return TEXT("vfx");
		case EProtoBoyAssetType::UI:              return TEXT("ui");
		case EProtoBoyAssetType::GameSystem:      return TEXT("game-system");
		case EProtoBoyAssetType::GameTemplate:    return TEXT("game-template");
		case EProtoBoyAssetType::ToolPlugin:      return TEXT("tool-and-plugin");
		case EProtoBoyAssetType::Tutorial:        return TEXT("tutorials-examples");
		default:                                  return FString();
		}
	}

	FSearch MakeSearch(const FProtoBoyAssetSearch& Search)
	{
		FSearch Out;
		Out.Query = Search.Query;
		for (const EProtoBoyAssetType Type : Search.Types)
		{
			const FString ListingType = GetListingType(Type);
			if (!ListingType.IsEmpty())
			{
				Out.ListingTypes.AddUnique(ListingType);
			}
		}
		Out.bFreeOnly = Search.bFreeOnly;
		Out.MinPrice = Search.MinPrice;
		Out.MaxPrice = Search.MaxPrice;
		return Out;
	}

	FString BuildSearchUrl(const FSearch& Search)
	{
		FString Url = FString::Printf(TEXT("https://www.fab.com/search?q=%s"), *FGenericPlatformHttp::UrlEncode(Search.Query.TrimStartAndEnd()));
		for (const FString& ListingType : Search.ListingTypes)
		{
			if (!ListingType.TrimStartAndEnd().IsEmpty())
			{
				Url += FString::Printf(TEXT("&listing_types=%s"), *FGenericPlatformHttp::UrlEncode(ListingType.TrimStartAndEnd()));
			}
		}
		// min_price / max_price checked on fab.com ("From $10.00 to $20.00"). Free only replaces the price range.
		if (!Search.bFreeOnly && Search.MinPrice.IsSet())
		{
			Url += FString::Printf(TEXT("&min_price=%s"), *FString::SanitizeFloat(Search.MinPrice.GetValue(), 0));
		}
		if (!Search.bFreeOnly && Search.MaxPrice.IsSet())
		{
			Url += FString::Printf(TEXT("&max_price=%s"), *FString::SanitizeFloat(Search.MaxPrice.GetValue(), 0));
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
