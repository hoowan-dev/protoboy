#pragma once

#include "Tasks/Trello/PBTrelloClient.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class SWidgetSwitcher;

/**
 * ProtoBoy dashboard: a sidebar listing the pages, and the selected page beside it. Pages: Setup (plugin settings, MCP,
 * Documentation, Tasks, Asset Sourcing), Documentation (the doc map) and Tasks (blank
 * for now). The open page is remembered per user.
 */
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
	/** Tasks: one row per task category on the doc map. Rebuilt on doc map changes. */
	void RebuildTasksSection();
	/** Task category titles, in doc map order. */
	TArray<FString> GetTaskCategoryNames() const;
	/**
	 * Compare the Trello board's To Do lists with the task categories (Sync with Trello). With bPromptIfOutOfSync, opens
	 * the out-of-sync window listing what a resync changes.
	 */
	void CheckTaskSync(bool bPromptIfOutOfSync);

	/** Whether the board's To Do lists match the task categories, shown next to the Sync button. */
	enum class ETaskSyncState : uint8
	{
		/** Not checked, or the categories changed since. */
		Unknown,
		Checking,
		InSync,
		OutOfSync,
		/** Trello couldn't be asked (TaskSyncError says why). */
		Failed,
	};
	ETaskSyncState TaskSyncState = ETaskSyncState::Unknown;
	FText TaskSyncError;
	/** The categories TaskSyncState was checked against. */
	TArray<FString> TaskSyncCategories;

	/** "Last synced <date>" (from the user's Trello connection) or "Never synced". */
	FText GetLastTaskSyncText() const;
	/** The board's To Do list for a category (any case), from the last sync; null if it has none or there was no sync. */
	const FProtoBoyTrelloCategorySync::FCategoryList* FindTaskList(const FString& Category) const;
	/** A category with its To Do count; opening it shows the tasks. Color is the layer strip on the left. */
	TSharedRef<SWidget> MakeTaskCategoryRow(const FString& Category, const FText& Detail, const FLinearColor& Color);
	/**
	 * One task: title (description as tooltip) and a link to the card. With bShowCategory, the task's category labels
	 * (Trello labels named after a task category, added by Sync) show as chips in the category's layer color.
	 */
	TSharedRef<SWidget> MakeTaskItem(const FProtoBoyTask& Task, bool bShowCategory = false);
	/** Open cards in the board's In Progress list (TrelloInProgressListId), as of the last sync. */
	TArray<FProtoBoyTask> InProgressTasks;
	/** Open cards in the board's Done list (TrelloDoneListId), most recent first, as of the last sync. */
	TArray<FProtoBoyTask> DoneTasks;
	/**
	 * Fill an In Progress / Done section: a note before the first sync or when empty, else the tasks with category chips.
	 * MaxShown > 0 lists only the first MaxShown, then says how many more there are.
	 */
	void FillTaskSection(const TSharedPtr<class SBox>& Box, const TArray<FProtoBoyTask>& Tasks, const FText& NotLoaded, const FText& Empty, int32 MaxShown = 0);
	/** Each category list's open cards, as of the last sync (Sync fills them). */
	TArray<FProtoBoyTrelloCategorySync::FCategoryList> TaskLists;
	bool bTaskListsLoaded = false;
	/** Opened categories (lowercase), kept across rebuilds. */
	TSet<FString> ExpandedTaskCategories;

	/** A sidebar entry and the page it opens. */
	struct FPage
	{
		FText Label;
		FText Description;
		FName Icon;
	};

	TSharedRef<ITableRow> OnGeneratePageRow(TSharedPtr<FPage> Page, const TSharedRef<STableViewBase>& OwnerTable);
	void OnPageSelected(TSharedPtr<FPage> Page, ESelectInfo::Type SelectInfo);

	/** Pages in sidebar order; the index matches the page's slot in PageSwitcher. */
	TArray<TSharedPtr<FPage>> Pages;
	TSharedPtr<SListView<TSharedPtr<FPage>>> PageList;
	TSharedPtr<SWidgetSwitcher> PageSwitcher;
	int32 ActivePage = 0;

	/** A setup check: returns true when done and fills OutDetail with what's set up or what's missing. */
	using FSetupStepState = TFunction<bool(FText& OutDetail)>;

	/**
	 * The setup checks, shown in the Setup page's card headers and counted in the sidebar.
	 * Built once in Construct.
	 */
	void BuildSetupSteps();
	FSetupStepState AgentStep;
	FSetupStepState DocsStep;
	FSetupStepState TasksStep;
	/** Asset sourcing is optional, so always done; the detail describes the team's choices. */
	FSetupStepState SourcingStep;
	FSetupStepState DocHealthStep;

	/** Wraps Body in a collapsible, titled card. Use for every section inside a page so they look consistent. */
	TSharedRef<SWidget> MakeSection(const FText& Title, const TSharedRef<SWidget>& Body);
	/** Same card with a check or red X before the title, from Status (its detail is the icon's tooltip). */
	TSharedRef<SWidget> MakeSection(const FText& Title, FSetupStepState Status, const TSharedRef<SWidget>& Body);

	/** Setup: plugin settings and the connections to agents, docs, task boards and asset sources. */
	TSharedRef<SWidget> MakeSetupSection();

	/** Setup > MCP: a logo chip for each agent app this user has set up (Claude Code, Claude Desktop, Gemini CLI). */
	TSharedRef<SWidget> MakeEnabledAgents();

	/** Documentation: the doc map, disabled with a warning until setup is finished. */
	TSharedRef<SWidget> MakeDocumentationSection();

	/** Tasks: the Sync row, then To Do (the task categories), In Progress and Done sections; a warning until setup is finished. */
	TSharedRef<SWidget> MakeTasksSection();

	/**
	 * What the Documentation and Tasks pages wait on: the agent (MCP), the docs connection and the task board. Empty
	 * when setup is done. A missing Game doc doesn't count, since the doc map is where it gets created.
	 */
	TArray<FText> GetMissingSetup() const;
	bool IsSetupDone() const { return GetMissingSetup().IsEmpty(); }
	/** Warning row shown while setup isn't done: LockedFormat with {0} = what's missing, plus a Go to Setup button. */
	TSharedRef<SWidget> MakeSetupLock(const FText& LockedFormat);

	
	/** The checklist's steps, in order (also used for the "N of M" count). */
	TArray<FSetupStepState> SetupSteps;
	int32 CountDoneSetupSteps() const;

	/** Whether the Game doc exists. Cached; refreshed with the Documentation section and on doc map changes. */
	bool bHasGameDoc = false;

	FReply OnOpenPluginSettingsClicked();
	FReply OnConnectDocsClicked();
	FReply OnMCPSettingsClicked();
	FReply OnTasksSettingsClicked();
	/** Opens the Setup page, where Asset Sourcing is edited. */

	TSharedPtr<class SBox> DocsSection;
	/** Tasks page: the To Do section's category rows and the In Progress section's tasks. */
	TSharedPtr<class SBox> TasksList;
	TSharedPtr<class SBox> InProgressList;
	TSharedPtr<class SBox> DoneList;
	/** Setup > Asset Sourcing; refreshed with the docs section. */
	TSharedPtr<class SProtoBoyAssetSourcingPanel> AssetSourcingPanel;
	FDelegateHandle ConnectionChangedHandle;
	FDelegateHandle DocMapChangedHandle;
};
