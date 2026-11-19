#pragma once

#include "Containers/Ticker.h"
#include "Health/PBDocHealthReport.h"
#include "Health/PBDocStructure.h"
#include "MCP/PBMCPConnection.h"
#include "Widgets/SCompoundWidget.h"

class FProtoBoyDocHealthRun;
class SScrollBox;
class SVerticalBox;

/**
 * The dashboard's Health page, four sections:
 * - Documentation Health Checks: what the checks do, the revision control status, the agent to use and Run Health
 *   Checks (Cancel while one runs), which starts an FProtoBoyDocHealthRun.
 * - Agent Log: the run's progress, once one has started.
 * - Docs That May Need Updating: the last report (this session's, or the saved one from
 *   ProtoBoyDocHealthReport::Load), one card per doc with the agent's reason, the changed files behind it, Open Doc
 *   and Mark as Up to Date (FProtoBoyDocMap::MarkReviewed, which takes the doc off the report).
 * - Documentation Structure: what ProtoBoyDocStructure::Check finds (broken links, missing paths, unwritten sections,
 *   coverage gaps), one card per issue. Needs no agent and no revision control, so it shows even while the page is
 *   locked. Checked when the page is first shown and after every doc map change; Check Again re-reads the docs.
 * The first three are locked (LockMessage shows instead) while IsUnlocked is false: until Enable documentation health
 * checks is ticked in Setup > Documentation Health Checks (SProtoBoyDocHealthPanel) and revision control is connected
 * (ProtoBoyDocHealth::IsEnabled).
 */
class SProtoBoyHealthPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProtoBoyHealthPage)
		: _IsUnlocked(true)
	{}
		/** False until health checks are enabled; the page then shows only LockMessage. */
		SLATE_ATTRIBUTE(bool, IsUnlocked)
		/** Why the page is locked, and where to fix it (the dashboard's setup warning). */
		SLATE_NAMED_SLOT(FArguments, LockMessage)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SProtoBoyHealthPage() override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	/** What the checks do, the revision control status, the agent picker and the Run Health Checks button. */
	TSharedRef<SWidget> MakeChecks();
	/** The agent's progress, one line per step, in a scrollable box. */
	TSharedRef<SWidget> MakeAgentLog();
	/** The report's summary line and its docs (ShowReport). */
	TSharedRef<SWidget> MakeResults();
	TSharedRef<SWidget> MakeStaleDoc(const FProtoBoyStaleDoc& Doc);
	/** Rebuild the result cards from Report. */
	void ShowReport();
	FText GetReportSummary() const;

	void StartRun();
	/** Core ticker: read the agent's output while it runs (the page may be hidden behind another page). */
	bool PollRun(float DeltaTime);
	bool IsChecking() const;
	/** Open a doc in Obsidian when connected, else in the default Markdown app. OutStatus gets the error, or is cleared. */
	void OpenDoc(const FString& DocId, const FString& Path, const FString& Title, FText& OutStatus);
	/** The doc is fine as it is: stamp it as reviewed and take it off the report. */
	void MarkUpToDate(const FProtoBoyStaleDoc& Doc);

	/** What the structure checks look for, Check Again, the summary and the issues (ShowStructure). */
	TSharedRef<SWidget> MakeStructure();
	TSharedRef<SWidget> MakeIssue(const FProtoBoyDocIssue& Issue);
	/** Run the structure checks the next time the page is on screen (from Tick: hidden pages don't tick). */
	void RequestStructureCheck();
	/** Rebuild the issue cards from Issues. */
	void ShowStructure();
	FText GetStructureSummary() const;

	TAttribute<bool> IsUnlocked;
	/** The agent that judges the docs (SProtoBoyAgentPicker). */
	TOptional<EProtoBoyMCPClient> SelectedAgent;
	/** The last run's outcome, or why it couldn't start. */
	FText RunStatus;
	bool bRunFailed = false;

	/** The last run, kept after it finishes for its log. */
	TSharedPtr<FProtoBoyDocHealthRun> Run;
	FTSTicker::FDelegateHandle PollHandle;
	TSharedPtr<SScrollBox> LogScroll;
	int32 LogShown = 0;

	/** The last report: this session's run, or the one saved by an earlier session. */
	FProtoBoyDocHealthReport Report;
	bool bHasReport = false;
	TSharedPtr<SVerticalBox> ResultList;
	/** Errors from opening a doc or marking it as up to date. */
	FText ResultStatus;

	/** The last structure check's findings. */
	TArray<FProtoBoyDocIssue> Issues;
	/** The structure checks have run at least once. */
	bool bStructureChecked = false;
	/** The docs changed (or were never checked): check again the next time the page ticks. */
	bool bStructureCheckPending = false;
	TSharedPtr<SVerticalBox> IssueList;
	/** Errors from opening a doc. */
	FText StructureStatus;
	FDelegateHandle DocMapChangedHandle;
};
