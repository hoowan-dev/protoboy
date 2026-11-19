#pragma once

#include "Widgets/SCompoundWidget.h"

class FProtoBoyDocHealthService;
class SScrollBox;
class SVerticalBox;
struct FProtoBoyDocIssue;
struct FProtoBoyStaleDoc;

/**
 * The dashboard's Health page, a view of FProtoBoyDocHealthService (the checks themselves live there, so they keep
 * running when the dashboard is closed and the status bar hub shows the same state). Four sections:
 * - Documentation Health Checks: what the checks do, the revision control status, the agent to use and Run Health
 *   Checks (Cancel while one runs).
 * - Agent Log: the check's progress, once one has started this session.
 * - Docs That May Need Updating: the last report, one card per doc with the agent's reason, the changed files behind
 *   it, Open Doc and Mark as Up to Date (which takes the doc off the report).
 * - Documentation Structure: what ProtoBoyDocStructure::Check finds (broken links, missing paths, unwritten sections,
 *   coverage gaps), one card per issue. Needs no agent and no revision control, so it shows even while the page is
 *   locked. Checked again whenever the page is on screen and the docs have changed; Check Again re-reads the docs.
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
		/** False until health checks are enabled; the page then shows only LockMessage and the structure checks. */
		SLATE_ATTRIBUTE(bool, IsUnlocked)
		/** Why the page is locked, and where to fix it (the dashboard's setup warning). */
		SLATE_NAMED_SLOT(FArguments, LockMessage)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SProtoBoyHealthPage() override;
	/** While on screen: show new log lines, and check the structure again if the docs changed. */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	/** Null only while the editor is shutting down. */
	static FProtoBoyDocHealthService* GetHealth();

	/** What the checks do, the revision control status, the agent picker and the Run Health Checks button. */
	TSharedRef<SWidget> MakeChecks();
	/** The agent's progress, one line per step, in a scrollable box. */
	TSharedRef<SWidget> MakeAgentLog();
	/** The report's summary line and its docs (ShowReport). */
	TSharedRef<SWidget> MakeResults();
	TSharedRef<SWidget> MakeStaleDoc(const FProtoBoyStaleDoc& Doc);
	/** What the structure checks look for, Check Again, the summary and the issues (ShowStructure). */
	TSharedRef<SWidget> MakeStructure();
	TSharedRef<SWidget> MakeIssue(const FProtoBoyDocIssue& Issue);
	FText GetStructureSummary() const;

	/** The service's state changed: rebuild the report and issue cards. */
	void OnHealthChanged();
	void ShowReport();
	void ShowStructure();

	/** Open a doc for editing. OutStatus gets the error, or is cleared. */
	void OpenDoc(const FString& DocId, const FString& Path, const FString& Title, FText& OutStatus);

	TAttribute<bool> IsUnlocked;
	FDelegateHandle HealthChangedHandle;

	TSharedPtr<SScrollBox> LogScroll;
	/** The check whose log is shown, and how many of its lines. */
	int32 ShownRunSerial = 0;
	int32 LogShown = 0;

	TSharedPtr<SVerticalBox> ResultList;
	/** Errors from opening a doc or marking it as up to date. */
	FText ResultStatus;

	TSharedPtr<SVerticalBox> IssueList;
	/** Errors from opening a doc. */
	FText StructureStatus;
};
