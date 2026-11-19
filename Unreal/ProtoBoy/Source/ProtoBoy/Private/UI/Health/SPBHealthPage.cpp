#include "UI/Health/SPBHealthPage.h"

#include "Docs/PBDocMap.h"
#include "HAL/PlatformProcess.h"
#include "Health/PBDocHealth.h"
#include "Health/PBDocHealthRun.h"
#include "MCP/PBMCPClientConfig.h"
#include "Misc/Paths.h"
#include "PBSubsystem.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "UI/Agents/SPBAgentPicker.h"
#include "UI/PBWidgets.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SProtoBoyHealthPage"

void SProtoBoyHealthPage::Construct(const FArguments& InArgs)
{
	IsUnlocked = InArgs._IsUnlocked;
	SelectedAgent = SProtoBoyAgentPicker::GetDefaultAgent();
	bHasReport = ProtoBoyDocHealthReport::Load(Report);

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return IsUnlocked.Get() ? EVisibility::Collapsed : EVisibility::Visible; })
			[
				InArgs._LockMessage.Widget
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return IsUnlocked.Get() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				ProtoBoyWidgets::MakeSection(LOCTEXT("ChecksHeader", "Documentation Health Checks"), MakeChecks())
			]
		]

		// Agent Log: only once a check has started.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 12.f, 0.f, 0.f)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return IsUnlocked.Get() && Run.IsValid() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				ProtoBoyWidgets::MakeSection(LOCTEXT("AgentLogHeader", "Agent Log"), MakeAgentLog())
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 12.f, 0.f, 0.f)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return IsUnlocked.Get() && bHasReport ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				ProtoBoyWidgets::MakeSection(LOCTEXT("ResultsHeader", "Docs That May Need Updating"), MakeResults())
			]
		]
	];

	ShowReport();
}

SProtoBoyHealthPage::~SProtoBoyHealthPage()
{
	if (PollHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PollHandle);
	}
	if (Run.IsValid())
	{
		Run->Cancel();
	}
}

TSharedRef<SWidget> SProtoBoyHealthPage::MakeChecks()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ChecksDesc", "Find the docs that may need updating: ProtoBoy lists the project files changed in revision control since each doc was last edited, and the agent judges which docs those changes outdate. Nothing is written to the docs."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text_Lambda([]() { FText Status; ProtoBoyDocHealth::IsRevisionControlConnected(Status); return Status; })
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 10.f, 0.f, 4.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("AgentLabel", "Agent"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.IsEnabled_Lambda([this]() { return !IsChecking(); })
			[
				SNew(SProtoBoyAgentPicker)
				.SelectedAgent_Lambda([this]() { return SelectedAgent; })
				.OnAgentPicked_Lambda([this](EProtoBoyMCPClient Agent) { SelectedAgent = Agent; })
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.Padding(0.f, 12.f, 0.f, 0.f)
		[
			SNew(SButton)
			.Text_Lambda([this]() { return IsChecking() ? LOCTEXT("CancelChecks", "Cancel") : LOCTEXT("RunChecks", "Run Health Checks"); })
			.ToolTipText_Lambda([this]()
			{
				return IsChecking()
					? LOCTEXT("CancelChecksTip", "Stop the agent. The last report stays.")
					: LOCTEXT("RunChecksTip", "List what changed since each doc was last edited, then have the agent find the docs that need updating. Reads Git history; the editor may pause for a moment.");
			})
			.OnClicked_Lambda([this]()
			{
				if (IsChecking())
				{
					Run->Cancel();
				}
				else
				{
					StartRun();
				}
				return FReply::Handled();
			})
		]

		// Status: a throbber while the agent runs, else the last run's outcome.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([this]() { return IsChecking() || !RunStatus.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(SCircularThrobber)
				.Radius(7.f)
				.Visibility_Lambda([this]() { return IsChecking() ? EVisibility::Visible : EVisibility::Collapsed; })
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					return IsChecking()
						? FText::Format(LOCTEXT("AgentChecking", "{0} is checking the docs..."), FProtoBoyMCPClientConfig::GetClientName(Run->GetClient()))
						: RunStatus;
				})
				.ColorAndOpacity_Lambda([this]() { return bRunFailed && !IsChecking() ? FSlateColor(FStyleColors::Error) : FSlateColor::UseSubduedForeground(); })
				.AutoWrapText(true)
			]
		];
}

TSharedRef<SWidget> SProtoBoyHealthPage::MakeAgentLog()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
		.Padding(8.f)
		[
			// Fixed height and its own scroll bar, so a long run doesn't push the results down the page.
			SNew(SBox)
			.HeightOverride(200.f)
			[
				SAssignNew(LogScroll, SScrollBox)
				.Orientation(Orient_Vertical)
				.ScrollBarAlwaysVisible(true)
			]
		];
}

TSharedRef<SWidget> SProtoBoyHealthPage::MakeResults()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 6.f)
		[
			SNew(STextBlock)
			.Text_Lambda([this]() { return GetReportSummary(); })
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		]

		// The agent didn't answer: the docs below are unjudged.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 6.f)
		[
			SNew(STextBlock)
			.Visibility_Lambda([this]() { return bHasReport && !Report.bAgentAnswered ? EVisibility::Visible : EVisibility::Collapsed; })
			.Text(LOCTEXT("Unjudged", "The agent didn't give an answer, so these are only the docs that name files changed after they were last edited. Run the checks again to have them judged."))
			.ColorAndOpacity(FStyleColors::Warning)
			.AutoWrapText(true)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(ResultList, SVerticalBox)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Visibility_Lambda([this]() { return ResultStatus.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
			.Text_Lambda([this]() { return ResultStatus; })
			.ColorAndOpacity(FStyleColors::Error)
			.AutoWrapText(true)
		];
}

FText SProtoBoyHealthPage::GetReportSummary() const
{
	if (!bHasReport)
	{
		return FText::GetEmpty();
	}
	// AsDateTime takes UTC and shows it in the user's time zone.
	const FText When = FText::AsDateTime(Report.CheckedUtc, EDateTimeStyle::Medium, EDateTimeStyle::Short);
	if (Report.AgentName.IsEmpty())
	{
		return FText::Format(LOCTEXT("SummaryNoChanges", "Last checked {0}: no project files had changed since the {1} {1}|plural(one=doc was,other=docs were) last edited."),
			When, Report.NumDocsChecked);
	}
	if (Report.Docs.IsEmpty())
	{
		return FText::Format(LOCTEXT("SummaryNone", "Last checked {0} with {1}: none of the {2} {2}|plural(one=doc,other=docs) {2}|plural(one=looks,other=look) out of date ({3} changed {3}|plural(one=file,other=files))."),
			When, FText::FromString(Report.AgentName), Report.NumDocsChecked, Report.NumChangedFiles);
	}
	return FText::Format(LOCTEXT("SummarySome", "Last checked {0} with {1}: {2} of {3} {3}|plural(one=doc,other=docs) may need updating ({4} changed {4}|plural(one=file,other=files)). Edit a doc and it drops off the next check."),
		When, FText::FromString(Report.AgentName), Report.Docs.Num(), Report.NumDocsChecked, Report.NumChangedFiles);
}

void SProtoBoyHealthPage::ShowReport()
{
	ResultList->ClearChildren();
	ResultStatus = FText::GetEmpty();
	if (!bHasReport)
	{
		return;
	}
	for (const FProtoBoyStaleDoc& Doc : Report.Docs)
	{
		ResultList->AddSlot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			MakeStaleDoc(Doc)
		];
	}
}

TSharedRef<SWidget> SProtoBoyHealthPage::MakeStaleDoc(const FProtoBoyStaleDoc& Doc)
{
	const bool bHigh = Doc.Confidence == TEXT("high");
	const FText Confidence = bHigh ? LOCTEXT("ConfidenceHigh", "Likely out of date")
		: Doc.Confidence == TEXT("medium") ? LOCTEXT("ConfidenceMedium", "Probably needs an update")
		: Doc.Confidence == TEXT("low") ? LOCTEXT("ConfidenceLow", "Worth a look")
		: LOCTEXT("ConfidenceNone", "Not judged");

	TSharedRef<SVerticalBox> Card = SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Doc.Title))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
				.AutoWrapText(true)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(Confidence)
				.ColorAndOpacity(bHigh ? FSlateColor(FStyleColors::Warning) : FSlateColor::UseSubduedForeground())
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("OpenDoc", "Open Doc"))
				.ToolTipText(FText::Format(LOCTEXT("OpenDocTip", "Open {0} to update it."), FText::FromString(Doc.Path)))
				.OnClicked_Lambda([this, Doc]() { OpenDoc(Doc); return FReply::Handled(); })
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 1.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("DocMeta", "{0}, last edited {1}"), FText::FromString(Doc.Path),
				FText::AsDateTime(Doc.DocModifiedUtc, EDateTimeStyle::Medium, EDateTimeStyle::Short)))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
			.AutoWrapText(true)
		];

	if (!Doc.Reason.IsEmpty())
	{
		Card->AddSlot()
		.AutoHeight()
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Doc.Reason))
			.AutoWrapText(true)
		];
	}
	if (!Doc.Files.IsEmpty())
	{
		Card->AddSlot()
		.AutoHeight()
		.Padding(0.f, 6.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ChangedFiles", "Files changed since:"))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
		Card->AddSlot()
		.AutoHeight()
		.Padding(0.f, 2.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Join(Doc.Files, TEXT("\n"))))
			.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
			.AutoWrapText(true)
		];
	}

	return SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		.Padding(8.f)
		[
			Card
		];
}

bool SProtoBoyHealthPage::IsChecking() const
{
	return Run.IsValid() && !Run->IsFinished();
}

void SProtoBoyHealthPage::StartRun()
{
	if (!SelectedAgent.IsSet())
	{
		bRunFailed = true;
		RunStatus = LOCTEXT("NoAgent", "Pick an agent to check the docs.");
		return;
	}

	const TSharedRef<FProtoBoyDocHealthRun> NewRun = MakeShared<FProtoBoyDocHealthRun>();
	FText Error;
	if (!NewRun->Start(SelectedAgent.GetValue(), Error))
	{
		bRunFailed = true;
		RunStatus = Error;
		return;
	}

	Run = NewRun;
	bRunFailed = false;
	RunStatus = FText::GetEmpty();
	LogShown = 0;
	LogScroll->ClearChildren();

	// The core ticker, not an active timer: the dashboard's other pages hide this one, and hidden widgets don't tick.
	if (PollHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PollHandle);
	}
	PollHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &SProtoBoyHealthPage::PollRun), 0.1f);
}

bool SProtoBoyHealthPage::PollRun(float DeltaTime)
{
	if (!Run.IsValid())
	{
		PollHandle.Reset();
		return false;
	}
	Run->Tick();

	const TArray<FString>& Log = Run->GetLog();
	if (LogShown < Log.Num())
	{
		for (; LogShown < Log.Num(); ++LogShown)
		{
			LogScroll->AddSlot()
			.Padding(0.f, 1.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Log[LogShown]))
				.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
				.AutoWrapText(true)
			];
		}
		LogScroll->ScrollToEnd();
	}
	if (!Run->IsFinished())
	{
		return true;
	}

	if (Run->Succeeded())
	{
		Report = Run->GetReport();
		bHasReport = true;
		ShowReport();
		bRunFailed = !Report.bAgentAnswered;
		RunStatus = Report.bAgentAnswered
			? LOCTEXT("RunDone", "Done. See Docs That May Need Updating below.")
			: LOCTEXT("RunNoAnswer", "The agent didn't give an answer. See the Agent Log for why.");
	}
	else
	{
		bRunFailed = false;
		RunStatus = LOCTEXT("RunCancelled", "Cancelled.");
	}
	PollHandle.Reset();
	return false;
}

void SProtoBoyHealthPage::OpenDoc(const FProtoBoyStaleDoc& Stale)
{
	const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	if (!Subsystem)
	{
		return;
	}
	// The report may be older than the map: the doc may have moved or gone since.
	const FProtoBoyDocNode* Doc = Subsystem->GetDocMap().Find(Stale.DocId);
	if (!Doc)
	{
		Doc = Subsystem->GetDocMap().FindByPath(Stale.Path);
	}
	if (!Doc)
	{
		ResultStatus = FText::Format(LOCTEXT("DocGone", "\"{0}\" isn't on the doc map any more."), FText::FromString(Stale.Title));
		return;
	}
	ResultStatus = FText::GetEmpty();
	if (Subsystem->GetObsidianConnector().IsConnected())
	{
		FText Error;
		if (!Subsystem->GetObsidianConnector().OpenInObsidian(Doc->Path, Error))
		{
			ResultStatus = Error;
		}
		return;
	}
	const FString File = FPaths::ConvertRelativePathToFull(Subsystem->GetDocsService().GetDocsRoot() / Doc->Path);
	FPlatformProcess::LaunchFileInDefaultExternalApplication(*File);
}

#undef LOCTEXT_NAMESPACE
