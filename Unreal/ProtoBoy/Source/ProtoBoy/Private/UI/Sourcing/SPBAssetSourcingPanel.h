#pragma once

#include "Widgets/SCompoundWidget.h"

/**
 * Setup > Asset Sourcing (optional) on the dashboard: whether agents should avoid AI-generated assets, which sources
 * are enabled (Fab today). Team-wide (UProtoBoySettings, Asset Sourcing); the style assets must have lives in Content
 * docs on the doc map.
 */
class SProtoBoyAssetSourcingPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProtoBoyAssetSourcingPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> MakeContent();

	/** Saves the team-wide setting and refreshes the agent guide, which states the policy. */
	void OnAssetSourcingChanged();
};
