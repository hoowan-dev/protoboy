#pragma once

#include "Tasks/Trello/PBTrelloClient.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SCompoundWidget.h"

class FProtoBoyTrelloConnector;
class SEditableTextBox;
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

	/** Template actions: shown when no board is chosen, or when the chosen board lacks lists. */
	TSharedRef<SWidget> MakeTemplateActions();
	/** Create a board from ProtoBoy's template (bNewBoard) or add the missing lists to the chosen board. */
	void ApplyTemplate(bool bNewBoard);
	/** Board chosen, lists loaded, and at least one of To Do / In Progress / Done isn't mapped to a list on the board. */
	bool NeedsListSetup() const;
	/** Template names of the lists the chosen board is missing, e.g. "In Progress, Done". */
	FString GetMissingListNames() const;
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

	TSharedPtr<SEditableTextBox> NewBoardNameBox;
	bool bApplyingTemplate = false;
	/** Result of the last template action. */
	FText TemplateMessage;

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
