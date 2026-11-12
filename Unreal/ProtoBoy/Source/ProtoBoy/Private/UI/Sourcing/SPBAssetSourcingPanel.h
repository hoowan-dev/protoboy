#pragma once

#include "Widgets/SCompoundWidget.h"

/**
 * Setup > Asset Sourcing (optional) on the dashboard: whether agents should avoid AI-generated assets, which sources
 * are enabled (Fab today), and the art direction doc. All team-wide (UProtoBoySettings, Asset Sourcing).
 */
class SProtoBoyAssetSourcingPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProtoBoyAssetSourcingPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Re-check whether the art direction doc exists (a file check, so cached; call when the docs folder changes). */
	void RefreshArtDirectionStatus();

private:
	TSharedRef<SWidget> MakeContent();

	/** Saves the team-wide setting and refreshes the agent guide, which states the policy. */
	void OnAssetSourcingChanged();

	FText ArtDirectionStatus;
};
