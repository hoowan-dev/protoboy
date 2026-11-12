#pragma once

#include "CoreMinimal.h"

/**
 * Pillar 3 (asset sourcing): where agents should get assets from, so they don't default to AI-generated ones.
 * Team-wide choices live in UProtoBoySettings (Asset Sourcing). Sources today: Fab.
 *
 * Fab has no public search API, and its site's internal endpoints aren't for third parties, so ProtoBoy only builds
 * fab.com search URLs and opens them in the user's browser. The user browses and picks; ProtoBoy never reads listings.
 */
namespace ProtoBoyAssetSourcing
{
	struct FFabSearch
	{
		FString Query;
		/** Fab "listing_types" filter, e.g. "3d-model", "material", "audio". Empty: all types. */
		FString ListingType;
		bool bFreeOnly = false;
	};

	PROTOBOY_API bool IsFabEnabled();

	/** https://www.fab.com/search?q=...&listing_types=...&asset_formats=...&is_free=1 (unset filters left out). */
	PROTOBOY_API FString BuildFabSearchUrl(const FFabSearch& Search);

	/** Open a Fab search in the user's browser. False with OutError if it couldn't be opened. */
	PROTOBOY_API bool OpenFabSearch(const FFabSearch& Search, FString& OutUrl, FText& OutError);

	/** Normalized art direction doc path, or empty if none is set. */
	PROTOBOY_API FString GetArtDirectionDocPath();

	/** Markdown for agents: the sourcing policy, enabled sources and where the art direction lives. */
	PROTOBOY_API FString DescribePolicyForAgents();
}
