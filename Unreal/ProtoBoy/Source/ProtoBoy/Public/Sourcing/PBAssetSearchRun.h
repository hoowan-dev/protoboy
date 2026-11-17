#pragma once

#include "CoreMinimal.h"
#include "Agents/PBAgentRun.h"
#include "Sourcing/PBAssetSourcing.h"

/** A specific listing on a source (a Fab listing page) that the agent found for a suggested asset. */
struct FProtoBoyAssetListing
{
	enum class EPreview : uint8
	{
		/** Reading the listing's page or downloading its image. */
		Loading,
		/** ImageData holds the preview image. */
		Ready,
		/** The listing exists but its preview couldn't be loaded. */
		None,
	};

	/** "Fab". */
	FString SourceName;
	/** The source's id for it (Fab: the listing's UUID). */
	FString Id;
	/** The listing's page, e.g. https://www.fab.com/listings/<id>. */
	FString Url;
	/** The agent's title, replaced by the listing page's own once it's read. */
	FString Title;
	/** The agent's one line on why it fits. */
	FString Note;
	FString ImageUrl;
	/** The cheapest license's current price from the listing's page (0: free); unset until read, or when it doesn't say. */
	TOptional<double> Price;
	/** ISO code, e.g. "USD". */
	FString Currency;
	/** The price before a sale, when there is one. */
	TOptional<double> ListPrice;
	/** The preview image as downloaded (JPEG/PNG), for the UI to decode. */
	TArray<uint8> ImageData;
	EPreview Preview = EPreview::Loading;
	/** The page doesn't exist (the agent got the URL wrong): not shown. */
	bool bNotFound = false;
	/** Its price is outside the user's filters (free only, price range): not shown. */
	bool bOutsideFilters = false;

	/** Shown in the UI: not missing and within the filters. */
	bool IsShown() const { return !bNotFound && !bOutsideFilters; }
};

/** One asset an agent suggests, with the specific listings it found for it. */
struct FProtoBoyAssetSuggestion
{
	/** Short name, e.g. "Wooden supply crates". */
	FString Name;
	/** What it's for and the style it must have, from the docs. */
	FString Description;
	/** Title of the Content doc it comes from; empty when it only comes from the user's request. */
	FString DocTitle;
	/** The user's filters (free only, price range) with the agent's query and types: "More on Fab" opens it. */
	FProtoBoyAssetSearch Search;
	/** In the agent's order, best first. */
	TArray<FProtoBoyAssetListing> Listings;
};

/**
 * Search For Assets with search context (Asset Sourcing page): runs one of the user's agents (FProtoBoyAgentRun) to turn
 * the user's request into a few suggested assets, each with specific listings it finds with web search (Fab listing
 * pages). With bUseDocsContext the prompt carries the Game doc and every Content doc (their style, What to Look For
 * and What to Avoid); with bUseProjectAssetsContext, a summary of /Game so suggestions match the project and skip what
 * it already has.
 *
 * Once the agent is done, each listing's page is read once, like a link preview (ProtoBoyFab::FetchListingPreview),
 * for its title and preview image; listings whose page doesn't exist are dropped. Tick() is polled by the page;
 * GetRevision() changes whenever the suggestions do. The prompt and raw output are kept in Saved/ProtoBoy/AssetSearch/.
 *
 * TODO: the images the docs embed (moodboards, references) aren't passed yet; the docs may live outside the project
 * folder, where the agent can't read them.
 */
class PROTOBOY_API FProtoBoyAssetSearchRun : public TSharedFromThis<FProtoBoyAssetSearchRun>
{
public:
	~FProtoBoyAssetSearchRun();

	/** Write the prompt and start the agent. False (nothing started) with a reason. Use a new run for each search. */
	bool Start(EProtoBoyMCPClient Client, const FProtoBoyAssetSearch& Search, FText& OutError);
	/** Read the agent's output, and once it exits, read the suggestions and start loading previews. Call often. */
	void Tick();
	/** Stop the agent, or stop loading previews. */
	void Cancel();

	bool IsRunning() const { return Agent.IsRunning(); }
	/** The agent is done (or was cancelled). Previews may still be loading. */
	bool IsFinished() const { return bFinished; }
	/** The agent suggested at least one asset. */
	bool Succeeded() const { return bSucceeded; }
	/** Stopped by Cancel before the agent was done. */
	bool WasCancelled() const { return bCancelled; }
	/** Listing pages or images still loading. */
	bool IsLoadingPreviews() const { return PendingLoads > 0; }
	EProtoBoyMCPClient GetClient() const { return Agent.GetClient(); }

	/** In the agent's order. */
	const TArray<FProtoBoyAssetSuggestion>& GetSuggestions() const { return Suggestions; }
	/** Changes whenever the suggestions or their listings do. */
	int32 GetRevision() const { return Revision; }
	const TArray<FString>& GetLog() const { return Agent.GetLog(); }

	/** Saved/ProtoBoy/AssetSearch/. */
	static FString GetWorkDir();

private:
	/** The agent exited: read the suggestions from its answer, then load the listings' previews. */
	void Complete();
	void LoadPreview(int32 SuggestionIndex, int32 ListingIndex);
	void LoadImage(int32 SuggestionIndex, int32 ListingIndex);
	/** One load finished; logs once the last one does. */
	void FinishLoad();
	FProtoBoyAssetListing* FindListing(int32 SuggestionIndex, int32 ListingIndex);

	FProtoBoyAgentRun Agent;
	/** The user's search; each suggestion starts from it. */
	FProtoBoyAssetSearch Request;
	TArray<FProtoBoyAssetSuggestion> Suggestions;
	int32 Revision = 0;
	int32 PendingLoads = 0;
	bool bFinished = false;
	bool bSucceeded = false;
	bool bCancelled = false;
};
