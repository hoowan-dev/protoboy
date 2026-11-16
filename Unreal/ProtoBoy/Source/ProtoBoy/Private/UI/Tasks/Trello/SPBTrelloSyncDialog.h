#pragma once

#include "Tasks/Trello/PBTrelloClient.h"
#include "Widgets/SCompoundWidget.h"

class SWindow;

/**
 * "Trello Board Out of Sync" window, opened by the Tasks page's Sync with Trello button when the board's
 * "To Do - <Category>" lists don't match the doc map's task categories. Lists what Resync will do (lists to add, lists
 * to delete and where their cards go, lists and labels to rename after their docs) and runs it with
 * FProtoBoyTaskBoard::ApplySync, which checks the board again afterwards.
 */
class SProtoBoyTrelloSyncDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProtoBoyTrelloSyncDialog) {}
		SLATE_ARGUMENT(FProtoBoyTrelloCategorySync, Plan)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Opens the window next to ParentWidget's window (not modal). Replaces one that's already open. */
	static void Open(const TSharedPtr<SWidget>& ParentWidget, const FProtoBoyTrelloCategorySync& Plan);

private:
	/** One line per change Resync makes, in the order it makes them. */
	TArray<FText> DescribeActions() const;
	FReply OnResyncClicked();
	void CloseWindow();

	FProtoBoyTrelloCategorySync Plan;
	bool bSyncing = false;
	FText ErrorText;

	static TWeakPtr<SWindow> OpenWindow;
};
