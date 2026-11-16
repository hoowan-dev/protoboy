#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class SWidgetSwitcher;

/**
 * ProtoBoy dashboard: a sidebar listing the pages, and the selected page beside it. Pages: Setup (plugin settings, MCP,
 * Documentation, Tasks, Asset Sourcing), Documentation (the doc map), Tasks (SProtoBoyTasksPage) and Asset Sourcing
 * (SProtoBoyAssetSourcingPage). The open page is remembered per user. Documentation, Tasks and Asset Sourcing stay
 * locked until setup is done; Asset Sourcing also needs its own setup (ProtoBoyAssetSourcing::GetMissingSetup).
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
	/** Done when sourced assets are preferred and at least one source is enabled (what the Asset Sourcing page needs). */
	FSetupStepState SourcingStep;
	FSetupStepState DocHealthStep;

	/** Wraps Body in a collapsible, titled card (ProtoBoyWidgets::MakeSection). */
	TSharedRef<SWidget> MakeSection(const FText& Title, const TSharedRef<SWidget>& Body);
	/** Same card with a check or red X before the title, from Status (its detail is the icon's tooltip). */
	TSharedRef<SWidget> MakeSection(const FText& Title, FSetupStepState Status, const TSharedRef<SWidget>& Body);

	/** Setup: plugin settings and the connections to agents, docs, task boards and asset sources. */
	TSharedRef<SWidget> MakeSetupSection();

	/** Setup > MCP: a logo chip for each agent app this user has set up (Claude Code, Claude Desktop, Gemini CLI). */
	TSharedRef<SWidget> MakeEnabledAgents();

	/** Documentation: the doc map, disabled with a warning until setup is finished. */
	TSharedRef<SWidget> MakeDocumentationSection();

	/** Tasks: SProtoBoyTasksPage, locked with a warning until setup is finished. */
	TSharedRef<SWidget> MakeTasksSection();

	/** Asset Sourcing: SProtoBoyAssetSourcingPage, locked until setup and the sourcing setup are finished. */
	TSharedRef<SWidget> MakeAssetSourcingSection();

	/**
	 * What the Documentation and Tasks pages wait on: the agent (MCP), the docs connection and the task board. Empty
	 * when setup is done. A missing Game doc doesn't count, since the doc map is where it gets created.
	 */
	TArray<FText> GetMissingSetup() const;
	bool IsSetupDone() const { return GetMissingSetup().IsEmpty(); }
	/** GetMissingSetup plus what asset sourcing needs (preferring sourced assets, an enabled source). */
	TArray<FText> GetMissingSourcingSetup() const;

	using FGetMissing = TFunction<TArray<FText>()>;
	/**
	 * Warning row shown while GetMissing returns anything: LockedFormat with {0} = what's missing, plus a Go to Setup
	 * button. Defaults to GetMissingSetup.
	 */
	TSharedRef<SWidget> MakeSetupLock(const FText& LockedFormat, FGetMissing GetMissing = nullptr);

	
	/** The checklist's steps, in order (also used for the "N of M" count). */
	TArray<FSetupStepState> SetupSteps;
	int32 CountDoneSetupSteps() const;

	/** Whether the Game doc exists. Cached; refreshed with the Documentation section and on doc map changes. */
	bool bHasGameDoc = false;

	FReply OnOpenPluginSettingsClicked();
	FReply OnConnectDocsClicked();
	FReply OnMCPSettingsClicked();
	FReply OnTasksSettingsClicked();

	TSharedPtr<class SBox> DocsSection;
	/** Setup > Asset Sourcing; refreshed with the docs section. */
	TSharedPtr<class SProtoBoyAssetSourcingPanel> AssetSourcingPanel;
	FDelegateHandle ConnectionChangedHandle;
	FDelegateHandle DocMapChangedHandle;
};
