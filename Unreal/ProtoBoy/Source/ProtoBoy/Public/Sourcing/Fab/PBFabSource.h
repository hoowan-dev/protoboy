#pragma once

#include "CoreMinimal.h"

/**
 * Fab (fab.com) as an asset source.
 *
 * Fab has no public search API, and its site's internal endpoints aren't for third parties, so ProtoBoy only builds
 * fab.com search URLs and opens them in the user's browser. The user browses and picks; ProtoBoy never reads listings.
 */
namespace ProtoBoyFab
{
	struct FSearch
	{
		FString Query;
		/** Fab "listing_types" filter, e.g. "3d-model", "material", "audio". Empty: all types. */
		FString ListingType;
		bool bFreeOnly = false;
	};

	/** Enabled as a source for this project (UProtoBoySettings::bEnableFabSource). */
	PROTOBOY_API bool IsEnabled();

	/** https://www.fab.com/search?q=...&listing_types=...&asset_formats=...&is_free=1 (unset filters left out). */
	PROTOBOY_API FString BuildSearchUrl(const FSearch& Search);

	/** Open a Fab search in the user's browser. False with OutError if Fab is disabled or the browser couldn't open. */
	PROTOBOY_API bool OpenSearch(const FSearch& Search, FString& OutUrl, FText& OutError);
}
