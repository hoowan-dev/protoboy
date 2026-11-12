#pragma once

#include "Widgets/SCompoundWidget.h"

/** ProtoBoy dashboard: Setup (plugin settings, MCP, Documentation, Tasks, Asset Sourcing) and Planning (setup checklist, ...) sections. */
class SProtoBoyDashboard : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProtoBoyDashboard) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SProtoBoyDashboard() override;

private:
	/** Rebuild the Documentation section from the current connection. Bound to OnConnectionChanged. */
	void RebuildDocsSection();

	/**
	 * Wraps Body in a collapsible, titled card. Use for every dashboard section so they look consistent.
	 * Top-level sections (Setup, Planning) get a larger header than the sections nested inside them.
	 */
	TSharedRef<SWidget> MakeSection(const FText& Title, const TSharedRef<SWidget>& Body, bool bTopLevel = false);


	/** Planning: a checklist of the setup steps, then (later) planning tools. */
	TSharedRef<SWidget> MakePlanningSection();

	/** One checklist row. GetState returns true when done and fills Detail; OnSetUp opens the window to finish it. */
	using FSetupStepState = TFunction<bool(FText& OutDetail)>;
	TSharedRef<SWidget> MakeSetupStep(const FText& Title, FSetupStepState GetState, FReply (SProtoBoyDashboard::*OnSetUp)());

	/** The checklist's steps, in order (also used for the "N of M" count). */
	TArray<FSetupStepState> SetupSteps;

	/** Whether the Architecture doc exists. A file check, so cached and refreshed with the Documentation section. */
	bool bHasArchitectureDoc = false;

	FReply OnOpenPluginSettingsClicked();
	FReply OnConnectDocsClicked();
	FReply OnMCPSettingsClicked();
	FReply OnTasksSettingsClicked();

	TSharedPtr<class SBox> DocsSection;
	/** Setup > Asset Sourcing; refreshed with the docs section. */
	TSharedPtr<class SProtoBoyAssetSourcingPanel> AssetSourcingPanel;
	FDelegateHandle ConnectionChangedHandle;
};
