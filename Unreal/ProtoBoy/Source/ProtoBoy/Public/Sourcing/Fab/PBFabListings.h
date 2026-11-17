#pragma once

#include "CoreMinimal.h"

/**
 * Specific Fab listings (https://www.fab.com/listings/<id>) for agent-suggested assets.
 *
 * Fab has no public API. Agents find listing URLs with web search; ProtoBoy then reads each listing's page once, the
 * way a link preview does, for its title and preview image (og:title, og:image). Never call Fab's internal endpoints or
 * crawl its search pages.
 */
namespace ProtoBoyFab
{
	/** What a listing page's preview tags say. */
	struct FListingPreview
	{
		FString Title;
		/** The listing's preview image (og:image, a JPEG on media.fab.com), or empty. */
		FString ImageUrl;
		/**
		 * The cheapest license's current price (the page's schema.org Product offer), 0 when free; unset when the page
		 * doesn't say. Other licenses (Professional) cost more.
		 */
		TOptional<double> Price;
		/** ISO code, e.g. "USD". */
		FString Currency;
		/** The price before a sale (the page's starting price), when it's above Price. */
		TOptional<double> ListPrice;
	};

	/** "Free", "$8.39" or "8.39 EUR". */
	PROTOBOY_API FString FormatPrice(double Price, const FString& Currency);

	enum class EListingFetch : uint8
	{
		Found,
		/** No such listing (404): usually a URL the agent got wrong. */
		NotFound,
		/** Offline, blocked or an unexpected answer; the listing may still exist. */
		Failed,
	};

	/**
	 * The listing id (a UUID, lowercase) in a fab.com listing URL: "https://www.fab.com/listings/<id>", with or without
	 * www, a locale ("/ja/listings/<id>"), a query or a fragment. False for anything else (search pages, other sites).
	 */
	PROTOBOY_API bool ParseListingUrl(const FString& Url, FString& OutId);

	/** https://www.fab.com/listings/<id>. */
	PROTOBOY_API FString MakeListingUrl(const FString& Id);

	/** Read a listing's page for its preview. OnDone runs on the game thread. */
	PROTOBOY_API void FetchListingPreview(const FString& Id, TFunction<void(EListingFetch Result, const FListingPreview& Preview)> OnDone);
}
