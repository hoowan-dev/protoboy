#pragma once

#include "Widgets/SCompoundWidget.h"

class FProtoBoyMCPServer;
struct FProtoBoyMCPTool;

/** "Server" page of the Connect LLM Provider window: server status, auth token, and which tools agents may call. */
class SProtoBoyMCPServerPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProtoBoyMCPServerPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> MakeStatusRow();
	TSharedRef<SWidget> MakeTokenRow();
	TSharedRef<SWidget> MakeToolRow(const FProtoBoyMCPTool& Tool);
	TSharedRef<SWidget> MakeLabel(const FText& Text);

	FReply OnStartStopClicked();
	FReply OnCopyTokenClicked();
	FReply OnRegenerateTokenClicked();
	FReply OnOpenProjectSettingsClicked();

	FText GetStatusText() const;
	FSlateColor GetStatusColor() const;

	static FProtoBoyMCPServer* GetServer();

	bool bShowToken = false;
	/** Set after a failed Start; cleared on the next Start/Stop. */
	FText StartError;
};
