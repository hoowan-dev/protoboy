#pragma once

#include "Containers/Ticker.h"
#include "MCP/PBMCPConnection.h"
#include "Sourcing/PBAssetSourcing.h"
#include "Widgets/SCompoundWidget.h"

class FProtoBoyAssetSearchRun;
struct FSlateDynamicImageBrush;
class SScrollBox;
class SVerticalBox;
struct FProtoBoyAssetListing;
struct FProtoBoyAssetSuggestion;

/**
 * The dashboard's Asset Sourcing page, two sections:
 * - Sources: every source ProtoBoy supports (ProtoBoyAssetSourcing::GetSources), and whether the team enabled it.
 * - Search For Assets: a query plus filters (asset types, free only, price range), opened on every enabled source.
 *   With a search context ticked (docs, project assets), the picked agent first turns it into suggested assets
 *   (FProtoBoyAssetSearchRun): an Agent Log section shows its progress, then Suggested Assets lists each one with the
 *   Fab listings the agent found, as tiles with their preview images.
 * Locked (only LockMessage shows) while IsUnlocked is false: the dashboard's general setup, plus preferring sourced
 * assets and at least one enabled source (ProtoBoyAssetSourcing::GetMissingSetup). Sources are enabled in
 * Setup > Asset Sourcing (SProtoBoyAssetSourcingPanel).
 */
class SProtoBoyAssetSourcingPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProtoBoyAssetSourcingPage)
		: _IsUnlocked(true)
	{}
		/** False until setup is done; the page then shows only LockMessage. */
		SLATE_ATTRIBUTE(bool, IsUnlocked)
		/** Why the page is locked, and where to fix it (the dashboard's setup warning). */
		SLATE_NAMED_SLOT(FArguments, LockMessage)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SProtoBoyAssetSourcingPage() override;

private:
	/** One row per supported source: icon, name, description, Enabled or Off. */
	TSharedRef<SWidget> MakeSources();
	/** The query box, the filters and the Search button. */
	TSharedRef<SWidget> MakeSearch();
	TSharedRef<SWidget> MakeTypeFilters();
	TSharedRef<SWidget> MakePriceFilters();
	/** The agent's progress, one line per step, in a scrollable box. */
	TSharedRef<SWidget> MakeAgentLog();
	/** The agent's suggestions, filled in once it's done (ShowSuggestions). */
	TSharedRef<SWidget> MakeSuggestions();
	TSharedRef<SWidget> MakeSuggestion(int32 SuggestionIndex);
	/** One listing: its preview image, title and the agent's note; clicking opens its page. Bound to the run's data. */
	TSharedRef<SWidget> MakeListingTile(int32 SuggestionIndex, int32 ListingIndex);
	const FProtoBoyAssetListing* FindListing(int32 SuggestionIndex, int32 ListingIndex) const;
	/** Decode the previews that have downloaded and have no brush yet (on a worker thread). */
	void DecodePreviews();
	void OnSearch();
	/** Start the agent on the search and its context. */
	void StartAgentSearch(const FProtoBoyAssetSearch& ToRun);
	/** Core ticker: read the agent's output while it runs (the page may be hidden behind another page). */
	bool PollRun(float DeltaTime);
	void ShowSuggestions();
	/** The agent is running or the listings' previews are loading: the search's inputs are locked, Search is Cancel. */
	bool IsSearching() const;
	/** The search's inputs can be changed (not IsSearching). */
	bool CanEditSearch() const { return !IsSearching(); }

	// TODO: list the asset needs from Content docs on the doc map (their What to Look For / What to Avoid), each with a
	// ready-made search, so the user doesn't have to write queries by hand.

	TAttribute<bool> IsUnlocked;
	/** The search being edited; Types empty means every type. */
	FProtoBoyAssetSearch Search;
	/** The agent that turns the query and the search context into search terms (SProtoBoyAgentPicker). */
	TOptional<EProtoBoyMCPClient> SelectedAgent;
	/** The last search's result: errors, or how many sources opened. */
	FText SearchStatus;
	bool bSearchFailed = false;

	/** The last agent search, kept after it finishes for its log and suggestions. */
	TSharedPtr<FProtoBoyAssetSearchRun> Run;
	FTSTicker::FDelegateHandle PollHandle;
	TSharedPtr<SScrollBox> LogScroll;
	int32 LogShown = 0;
	TSharedPtr<SVerticalBox> SuggestionList;
	/** The run's finish has been reported and the suggestions built. */
	bool bShownFinish = false;
	/** The run's revision the previews were last checked at. */
	int32 CheckedRevision = -1;
	/** Preview brushes by listing URL; null when the image couldn't be decoded. */
	TMap<FString, TSharedPtr<FSlateDynamicImageBrush>> PreviewBrushes;
	/** Listing URLs whose preview is being decoded. */
	TSet<FString> DecodingPreviews;
	/** The last suggestion opened: errors, or how many sources opened. */
	FText SuggestionStatus;
	bool bSuggestionFailed = false;
};
