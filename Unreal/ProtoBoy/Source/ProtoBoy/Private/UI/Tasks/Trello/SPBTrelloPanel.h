#pragma once

#include "Tasks/Trello/PBTrelloClient.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SCompoundWidget.h"

class FProtoBoyTrelloConnector;
class UProtoBoySettings;

/**
 * Trello page of the "Connect Task Board" window: one-click Connect via the browser, status, Disconnect,
 * and (once connected) the team's board and To Do / In Progress / Done list mapping.
 */
class SProtoBoyTrelloPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProtoBoyTrelloPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SProtoBoyTrelloPanel() override;

private:
	using FItemPtr = TSharedPtr<FProtoBoyTrelloItem>;
	using FListField = FString UProtoBoySettings::*;

	/** Board picker and list mapping, shown while connected. */
	TSharedRef<SWidget> MakeBoardSection();
	TSharedRef<SWidget> MakeListRow(const FText& Label, FListField Field);

	void RefreshBoards();
	void RefreshLists(const FString& BoardId);
	void OnBoardPicked(FItemPtr Board, ESelectInfo::Type SelectInfo);
	void OnConnectionChanged();

	/** Fill unmapped list fields by name ("To Do", "Doing", "Done", ...). */
	void AutoMapLists();
	FText GetListName(const FString& ListId) const;
	FText GetBoardStatusText() const;
	FSlateColor GetBoardStatusColor() const;

	TArray<FItemPtr> BoardOptions;
	TArray<FItemPtr> ListOptions;
	TSharedPtr<SComboBox<FItemPtr>> BoardCombo;
	TArray<TSharedPtr<SComboBox<FItemPtr>>> ListCombos;
	bool bLoadingBoards = false;
	bool bLoadingLists = false;
	FText BoardError;
	FDelegateHandle ConnectionChangedHandle;

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
