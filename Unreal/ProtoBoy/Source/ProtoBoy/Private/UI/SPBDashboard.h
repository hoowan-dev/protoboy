#pragma once

#include "Widgets/SCompoundWidget.h"

/** ProtoBoy dashboard: Setup (plugin settings, MCP, Documentation, Tasks) and Planning sections. */
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

	FReply OnOpenPluginSettingsClicked();
	FReply OnConnectDocsClicked();
	FReply OnMCPSettingsClicked();
	FReply OnTasksSettingsClicked();

	TSharedPtr<class SBox> DocsSection;
	FDelegateHandle ConnectionChangedHandle;
};
