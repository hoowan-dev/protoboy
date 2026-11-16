#pragma once

#include "Tasks/PBTaskBoard.h"
#include "Widgets/SCompoundWidget.h"

class SBox;

/**
 * The dashboard's Tasks page: the Sync row (Sync with the board, whether it matches the task categories, when it last
 * did, Generate Tasks), then To Do (one expandable row per task category, plus Uncategorized), In Progress and Done.
 * A view of FProtoBoyTaskBoard (the subsystem's): Sync fills it. Locked (only LockMessage shows) while IsUnlocked is false.
 */
class SProtoBoyTasksPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProtoBoyTasksPage)
		: _IsUnlocked(true)
	{}
		/** False until setup is done; the page then shows only LockMessage. */
		SLATE_ATTRIBUTE(bool, IsUnlocked)
		/** Why the page is locked, and where to fix it (the dashboard's setup warning). */
		SLATE_NAMED_SLOT(FArguments, LockMessage)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SProtoBoyTasksPage() override;

private:
	static FProtoBoyTaskBoard* GetBoard();
	static FProtoBoyTaskBoard::ESyncState GetSyncState();
	/** Check the board; when it's out of sync, open the window that lists what a resync changes. */
	void OnSyncClicked();
	/** Rebuild the rows and the In Progress and Done lists from the doc map and the board. */
	void RebuildTasksSection();
	/** "Last synced <date>" or "Never synced". */
	FText GetLastTaskSyncText() const;
	/** A category (DocId empty = Uncategorized) with its To Do count; opening it shows the tasks. Color is the layer strip. */
	TSharedRef<SWidget> MakeTaskCategoryRow(const FString& DocId, const FString& Category, const FText& Detail, const FLinearColor& Color);
	/**
	 * One task: title (description as tooltip) and a link to the card. With bShowCategory, the task's category labels
	 * (labels named after a task category, kept by Sync) show as chips in the category's layer color.
	 */
	TSharedRef<SWidget> MakeTaskItem(const FProtoBoyTask& Task, bool bShowCategory = false);
	/**
	 * Fill an In Progress / Done section: a note before the first sync or when empty, else the tasks with category chips.
	 * MaxShown > 0 lists only the first MaxShown, then says how many more there are.
	 */
	void FillTaskSection(const TSharedPtr<SBox>& Box, const TArray<FProtoBoyTask>& Tasks, const FText& NotLoaded, const FText& Empty, int32 MaxShown = 0);
	/** Opened categories (by doc id), kept across rebuilds. */
	TSet<FString> ExpandedTaskCategories;

	TAttribute<bool> IsUnlocked;
	/** The To Do section's category rows, and the In Progress and Done sections' tasks. */
	TSharedPtr<SBox> TasksList;
	TSharedPtr<SBox> InProgressList;
	TSharedPtr<SBox> DoneList;
	FDelegateHandle DocMapChangedHandle;
	FDelegateHandle BoardChangedHandle;
};
