#pragma once

#include "Tasks/Trello/PBTrelloClient.h"
#include "Widgets/SCompoundWidget.h"

class SWindow;

/**
 * "Trello Board Out of Sync" window, opened by the Tasks page's Sync with Trello button when the board's
 * "To Do - <Category>" lists don't match the doc map's task categories. Lists what Resync will do (lists to add, lists
 * to delete and where their cards go) and runs it with FProtoBoyTrelloClient::ApplyCategorySync.
 */
class SProtoBoyTrelloSyncDialog : public SCompoundWidget
{
public:
	/** Called after a resync attempt: true if every change was made. The window stays open on failure to show the error. */
	using FOnFinished = TFunction<void(bool bSynced)>;

	SLATE_BEGIN_ARGS(SProtoBoyTrelloSyncDialog) {}
		SLATE_ARGUMENT(FProtoBoyTrelloCategorySync, Plan)
		SLATE_ARGUMENT(FOnFinished, OnFinished)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Opens the window next to ParentWidget's window (not modal). Replaces one that's already open. */
	static void Open(const TSharedPtr<SWidget>& ParentWidget, const FProtoBoyTrelloCategorySync& Plan, FOnFinished OnFinished);

private:
	/** One line per change Resync makes, in the order it makes them. */
	TArray<FText> DescribeActions() const;
	FReply OnResyncClicked();
	void CloseWindow();

	FProtoBoyTrelloCategorySync Plan;
	FOnFinished OnFinished;
	bool bSyncing = false;
	FText ErrorText;

	static TWeakPtr<SWindow> OpenWindow;
};
