#pragma once

#include "Widgets/SCompoundWidget.h"

/** Placeholder dashboard: daemon status, PVI, health issues, docs actions. */
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

	/** Wraps Body in a collapsible, titled card. Use for every dashboard section so they look consistent. */
	TSharedRef<SWidget> MakeSection(const FText& Title, const TSharedRef<SWidget>& Body);

	FReply OnConnectDocsClicked();
	FReply OnMCPSettingsClicked();
	FReply OnTasksSettingsClicked();

	TSharedPtr<class SBox> DocsSection;
	FDelegateHandle ConnectionChangedHandle;
};
