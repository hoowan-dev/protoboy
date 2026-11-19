#pragma once

#include "Containers/Ticker.h"
#include "Health/PBDocHealthReport.h"
#include "MCP/PBMCPConnection.h"
#include "Widgets/SCompoundWidget.h"

class FProtoBoyDocHealthRun;
class SScrollBox;
class SVerticalBox;

/**
 * The dashboard's Health page, three sections:
 * - Documentation Health Checks: what the checks do, the revision control status, the agent to use and Run Health
 *   Checks (Cancel while one runs), which starts an FProtoBoyDocHealthRun.
 * - Agent Log: the run's progress, once one has started.
 * - Docs That May Need Updating: the last report (this session's, or the saved one from
 *   ProtoBoyDocHealthReport::Load), one card per doc with the agent's reason, the changed files behind it and Open Doc.
 * Locked (only LockMessage shows) while IsUnlocked is false: until Enable documentation health checks is ticked in
 * Setup > Documentation Health Checks (SProtoBoyDocHealthPanel) and revision control is connected
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
	/** Open a doc in Obsidian when connected, else in the default Markdown app. */
	void OpenDoc(const FProtoBoyStaleDoc& Doc);

	// TODO: "Mark as up to date" per doc (a reviewed date in its frontmatter), so a doc that was checked and is fine
	// stops being flagged for the same changes.

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
	/** Errors from opening a doc. */
	FText ResultStatus;
};
