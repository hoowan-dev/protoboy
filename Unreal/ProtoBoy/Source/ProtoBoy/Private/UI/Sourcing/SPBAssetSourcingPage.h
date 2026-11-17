#pragma once

#include "MCP/PBMCPConnection.h"
#include "Sourcing/PBAssetSourcing.h"
#include "Widgets/SCompoundWidget.h"

/**
 * The dashboard's Asset Sourcing page, two sections:
 * - Sources: every source ProtoBoy supports (ProtoBoyAssetSourcing::GetSources), and whether the team enabled it.
 * - Search For Assets: a query plus filters (asset types, free only, price range), opened on every enabled source.
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

private:
	/** One row per supported source: icon, name, description, Enabled or Off. */
	TSharedRef<SWidget> MakeSources();
	/** The query box, the filters and the Search button. */
	TSharedRef<SWidget> MakeSearch();
	TSharedRef<SWidget> MakeTypeFilters();
	TSharedRef<SWidget> MakePriceFilters();
	void OnSearch();

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
};
