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

	FReply OnConnectDocsClicked();

	TSharedPtr<class SBox> DocsSection;
	FDelegateHandle ConnectionChangedHandle;
};
