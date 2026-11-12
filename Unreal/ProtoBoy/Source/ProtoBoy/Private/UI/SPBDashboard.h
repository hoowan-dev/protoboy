#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class SWidgetSwitcher;

/**
 * ProtoBoy dashboard: a sidebar listing the pages, and the selected page beside it. Pages: Setup (plugin settings, MCP,
 * Documentation, Tasks, Asset Sourcing), Architecture (setup checklist, pillar tree), Documentation and Tasks (both blank
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
	 * The setup checks, shared by the Setup page's card headers and the Architecture page's checklist so both always agree.
	 * Built once in Construct.
	 */
	void BuildSetupSteps();
	FSetupStepState AgentStep;
	FSetupStepState DocsStep;
	FSetupStepState TasksStep;
	/** Asset sourcing is optional, so always done; the detail describes the team's choices. */
	FSetupStepState SourcingStep;

	/** Wraps Body in a collapsible, titled card. Use for every section inside a page so they look consistent. */
	TSharedRef<SWidget> MakeSection(const FText& Title, const TSharedRef<SWidget>& Body);
	/** Same card with a check or red X before the title, from Status (its detail is the icon's tooltip). */
	TSharedRef<SWidget> MakeSection(const FText& Title, FSetupStepState Status, const TSharedRef<SWidget>& Body);

	/** Setup: plugin settings and the connections to agents, docs, task boards and asset sources. */
	TSharedRef<SWidget> MakeSetupSection();

	/** Setup > MCP: a logo chip for each agent app this user has set up (Claude Code, Claude Desktop, Gemini CLI). */
	TSharedRef<SWidget> MakeEnabledAgents();

	/** Architecture: a checklist of the setup steps, then the pillar tree editor. */
	TSharedRef<SWidget> MakeArchitectureSection();

	/** Documentation: blank for now. */
	TSharedRef<SWidget> MakeDocumentationSection();

	/** Tasks: blank for now. */
	TSharedRef<SWidget> MakeTasksSection();

	/**
	 * One checklist row. GetState returns true when done and fills Detail; OnSetUp opens the window to finish it.
	 * The "Set Up..." button hides once the step is done, unless ChangeText is given (then it's always shown with that label).
	 */
	TSharedRef<SWidget> MakeSetupStep(const FText& Title, FSetupStepState GetState, FReply (SProtoBoyDashboard::*OnSetUp)(), const FText& ChangeText = FText::GetEmpty());

	/** The checklist's steps, in order (also used for the "N of M" count). */
	TArray<FSetupStepState> SetupSteps;
	int32 CountDoneSetupSteps() const;

	/** Whether the Architecture doc exists. A file check, so cached and refreshed with the Documentation section. */
	bool bHasArchitectureDoc = false;

	FReply OnOpenPluginSettingsClicked();
	FReply OnConnectDocsClicked();
	FReply OnMCPSettingsClicked();
	FReply OnTasksSettingsClicked();
	/** Opens the Setup page, where Asset Sourcing is edited. */
	FReply OnAssetSourcingSettingsClicked();

	TSharedPtr<class SBox> DocsSection;
	/** Setup > Asset Sourcing; refreshed with the docs section. */
	TSharedPtr<class SProtoBoyAssetSourcingPanel> AssetSourcingPanel;
	FDelegateHandle ConnectionChangedHandle;
};
