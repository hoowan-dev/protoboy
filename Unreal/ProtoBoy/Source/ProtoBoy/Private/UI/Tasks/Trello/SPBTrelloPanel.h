#pragma once

#include "Widgets/SCompoundWidget.h"

class FProtoBoyTrelloConnector;

/** Trello page of the "Connect Task Board" window: one-click Connect via the browser, status, and Disconnect. */
class SProtoBoyTrelloPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProtoBoyTrelloPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> MakeHeader();
	/** Step-by-step guide to getting a Trello API key (expanded until a key is set). */
	TSharedRef<SWidget> MakeSetupDetails();
	TSharedRef<SWidget> MakeStep(int32 Number, const FText& Title, const TSharedRef<SWidget>& Body);

	FReply OnConnectClicked();
	FReply OnCancelClicked();
	FReply OnDisconnectClicked();
	FReply OnCopyOriginClicked();
	FReply OnOpenProjectSettingsClicked();

	FText GetStatusText() const;
	FSlateColor GetStatusColor() const;

	static FProtoBoyTrelloConnector* GetConnector();

	/** Set when Connect can't start (no key, port busy); cleared on the next attempt. */
	FText StartError;
};
