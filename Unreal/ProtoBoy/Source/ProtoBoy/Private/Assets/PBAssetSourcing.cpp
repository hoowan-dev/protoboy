#include "Assets/PBAssetSourcing.h"

#include "Docs/PBDocGenerator.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HAL/PlatformProcess.h"
#include "PBSettings.h"

#define LOCTEXT_NAMESPACE "ProtoBoyAssetSourcing"

namespace ProtoBoyAssetSourcing
{
	bool IsFabEnabled()
	{
		return GetDefault<UProtoBoySettings>()->bEnableFabSource;
	}

	FString BuildFabSearchUrl(const FFabSearch& Search)
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

	bool OpenFabSearch(const FFabSearch& Search, FString& OutUrl, FText& OutError)
	{
		if (!IsFabEnabled())
		{
			OutError = LOCTEXT("FabDisabled", "Fab isn't enabled as an asset source for this project.");
			return false;
		}
		if (Search.Query.TrimStartAndEnd().IsEmpty())
		{
			OutError = LOCTEXT("NoQuery", "Enter what to search for.");
			return false;
		}

		OutUrl = BuildFabSearchUrl(Search);
		FString LaunchError;
		FPlatformProcess::LaunchURL(*OutUrl, nullptr, &LaunchError);
		if (!LaunchError.IsEmpty())
		{
			OutError = FText::Format(LOCTEXT("LaunchFailed", "Couldn't open the browser: {0}"), FText::FromString(LaunchError));
			return false;
		}
		return true;
	}

	FString GetArtDirectionDocPath()
	{
		const FString& Configured = GetDefault<UProtoBoySettings>()->ArtDirectionDocPath;
		FString Normalized;
		FText Ignored;
		return !Configured.TrimStartAndEnd().IsEmpty() && FProtoBoyDocGenerator::NormalizeDocPath(Configured, Normalized, Ignored)
			? Normalized
			: FString();
	}

	FString DescribePolicyForAgents()
	{
		const UProtoBoySettings* Settings = GetDefault<UProtoBoySettings>();

		FString Out;
		if (Settings->bPreferSourcedAssets)
		{
			Out += TEXT("- **Policy:** don't generate art, audio or other assets with AI. Use the sources below, or clearly "
				"marked placeholders (greybox shapes, temp audio), and ask the user before anything else.\n");
		}
		else
		{
			Out += TEXT("- **Policy:** no restriction set. Still prefer the sources below where they fit.\n");
		}

		TArray<FString> Sources;
		if (Settings->bEnableFabSource)
		{
			Sources.Add(TEXT("Fab (fab_search: opens a search in the user's browser; the user picks listings)"));
		}
		Out += FString::Printf(TEXT("- **Sources:** %s\n"), Sources.IsEmpty() ? TEXT("none enabled (ask the user where assets should come from)") : *FString::Join(Sources, TEXT("; ")));

		const FString ArtDoc = GetArtDirectionDocPath();
		Out += ArtDoc.IsEmpty()
			? FString(TEXT("- **Art direction:** no doc is set; search the docs (docs_search \"art style\") or ask the user before choosing assets.\n"))
			: FString::Printf(TEXT("- **Art direction:** read %s (docs_read) before choosing assets; matches must fit its style and goals.\n"), *ArtDoc);
		return Out;
	}
}

#undef LOCTEXT_NAMESPACE
