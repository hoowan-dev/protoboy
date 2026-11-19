#include "UI/Health/SPBHealthPage.h"

#include "Docs/PBDocMap.h"
#include "Health/PBDocHealth.h"
#include "Health/PBDocHealthService.h"
#include "MCP/PBMCPClientConfig.h"
#include "PBSubsystem.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "UI/Agents/SPBAgentPicker.h"
#include "UI/PBWidgets.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SProtoBoyHealthPage"

FProtoBoyDocHealthService* SProtoBoyHealthPage::GetHealth()
{
	UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	return Subsystem ? &Subsystem->GetDocHealth() : nullptr;
}

void SProtoBoyHealthPage::Construct(const FArguments& InArgs)
{
	IsUnlocked = InArgs._IsUnlocked;

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

		// Agent Log: only once a check has started this session.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 12.f, 0.f, 0.f)
		[
			SNew(SBox)
			.Visibility_Lambda([this]()
			{
				const FProtoBoyDocHealthService* Health = GetHealth();
				return IsUnlocked.Get() && Health && Health->GetRunSerial() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
			})
			[
				ProtoBoyWidgets::MakeSection(LOCTEXT("AgentLogHeader", "Agent Log"), MakeAgentLog())
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 12.f, 0.f, 0.f)
		[
			SNew(SBox)
			.Visibility_Lambda([this]()
			{
				const FProtoBoyDocHealthService* Health = GetHealth();
				return IsUnlocked.Get() && Health && Health->HasReport() ? EVisibility::Visible : EVisibility::Collapsed;
			})
			[
				ProtoBoyWidgets::MakeSection(LOCTEXT("ResultsHeader", "Docs That May Need Updating"), MakeResults())
			]
		]

		// Needs neither the agent nor revision control, so it isn't behind the lock.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 12.f, 0.f, 0.f)
		[
			ProtoBoyWidgets::MakeSection(LOCTEXT("StructureHeader", "Documentation Structure"), MakeStructure())
		]
	];

	if (FProtoBoyDocHealthService* Health = GetHealth())
	{
		HealthChangedHandle = Health->OnChanged.AddSP(this, &SProtoBoyHealthPage::OnHealthChanged);
	}
	OnHealthChanged();
}

SProtoBoyHealthPage::~SProtoBoyHealthPage()
{
	// The check keeps running: it belongs to the service, and the status bar hub still shows it.
	if (FProtoBoyDocHealthService* Health = GetHealth())
	{
		Health->OnChanged.Remove(HealthChangedHandle);
	}
}

void SProtoBoyHealthPage::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	FProtoBoyDocHealthService* Health = GetHealth();
	if (!Health)
	{
		return;
	}

	// A new check (started here or from the status bar hub) starts a new log.
	if (Health->GetRunSerial() != ShownRunSerial)
	{
		ShownRunSerial = Health->GetRunSerial();
		LogShown = 0;
		LogScroll->ClearChildren();
	}
	const TArray<FString>& Log = Health->GetLog();
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

	// Only ticked while on screen, so a hidden page doesn't re-read every doc after each map change.
	if (Health->IsStructureStale())
	{
		Health->CheckStructure();
	}
}

void SProtoBoyHealthPage::OnHealthChanged()
{
	ShowReport();
	ShowStructure();
}

// --- The check against revision control ---------------------------------------------------------------------------

TSharedRef<SWidget> SProtoBoyHealthPage::MakeChecks()
{
	auto IsChecking = []()
	{
		const FProtoBoyDocHealthService* Health = GetHealth();
		return Health && Health->IsChecking();
	};

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ChecksDesc", "Find the docs that may need updating: ProtoBoy lists the project files changed in revision control since each doc was last edited, and the agent judges which docs those changes outdate. You can also run this from the ProtoBoy button in the status bar."))
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

		// Shared with the status bar hub: it runs checks with the agent picked here.
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.IsEnabled_Lambda([IsChecking]() { return !IsChecking(); })
			[
				SNew(SProtoBoyAgentPicker)
				.SelectedAgent_Lambda([]()
				{
					const FProtoBoyDocHealthService* Health = GetHealth();
					return Health ? Health->GetAgent() : TOptional<EProtoBoyMCPClient>();
				})
				.OnAgentPicked_Lambda([](EProtoBoyMCPClient Agent)
				{
					if (FProtoBoyDocHealthService* Health = GetHealth())
					{
						Health->SetAgent(Agent);
					}
				})
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.Padding(0.f, 12.f, 0.f, 0.f)
		[
			SNew(SButton)
			.Text_Lambda([IsChecking]() { return IsChecking() ? LOCTEXT("CancelChecks", "Cancel") : LOCTEXT("RunChecks", "Run Health Checks"); })
			.ToolTipText_Lambda([IsChecking]()
			{
				return IsChecking()
					? LOCTEXT("CancelChecksTip", "Stop the agent. The last report stays.")
					: LOCTEXT("RunChecksTip", "List what changed since each doc was last edited, then have the agent find the docs that need updating. Reads Git history; the editor may pause for a moment.");
			})
			.OnClicked_Lambda([IsChecking]()
			{
				if (FProtoBoyDocHealthService* Health = GetHealth())
				{
					if (IsChecking())
					{
						Health->CancelCheck();
					}
					else
					{
						// Why it couldn't start is kept as the service's run status, shown below.
						FText Error;
						Health->StartCheck(Error);
					}
				}
				return FReply::Handled();
			})
		]

		// Status: a throbber while the agent runs, else how the last check ended.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([IsChecking]()
			{
				const FProtoBoyDocHealthService* Health = GetHealth();
				return Health && (IsChecking() || !Health->GetRunStatus().IsEmpty()) ? EVisibility::Visible : EVisibility::Collapsed;
			})

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(SCircularThrobber)
				.Radius(7.f)
				.Visibility_Lambda([IsChecking]() { return IsChecking() ? EVisibility::Visible : EVisibility::Collapsed; })
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([IsChecking]()
				{
					const FProtoBoyDocHealthService* Health = GetHealth();
					if (!Health)
					{
						return FText::GetEmpty();
					}
					return IsChecking()
						? FText::Format(LOCTEXT("AgentChecking", "{0} is checking the docs..."), FProtoBoyMCPClientConfig::GetClientName(Health->GetCheckingAgent()))
						: Health->GetRunStatus();
				})
				.ColorAndOpacity_Lambda([IsChecking]()
				{
					const FProtoBoyDocHealthService* Health = GetHealth();
					return Health && Health->DidRunFail() && !IsChecking() ? FSlateColor(FStyleColors::Error) : FSlateColor::UseSubduedForeground();
				})
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
			.Text_Lambda([]()
			{
				const FProtoBoyDocHealthService* Health = GetHealth();
				return Health ? Health->DescribeReport() : FText::GetEmpty();
			})
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		]

		// The agent didn't answer: the docs below are unjudged.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 6.f)
		[
			SNew(STextBlock)
			.Visibility_Lambda([]()
			{
				const FProtoBoyDocHealthService* Health = GetHealth();
				return Health && Health->HasReport() && !Health->GetReport().bAgentAnswered ? EVisibility::Visible : EVisibility::Collapsed;
			})
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

void SProtoBoyHealthPage::ShowReport()
{
	ResultList->ClearChildren();
	ResultStatus = FText::GetEmpty();
	const FProtoBoyDocHealthService* Health = GetHealth();
	if (!Health || !Health->HasReport())
	{
		return;
	}
	for (const FProtoBoyStaleDoc& Doc : Health->GetReport().Docs)
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
	// Copies for the buttons: the report can change under them.
	const FString DocId = Doc.DocId;
	const FString DocPath = Doc.Path;
	const FString DocTitle = Doc.Title;

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
				.OnClicked_Lambda([this, DocId, DocPath, DocTitle]() { OpenDoc(DocId, DocPath, DocTitle, ResultStatus); return FReply::Handled(); })
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("MarkUpToDate", "Mark as Up to Date"))
				.ToolTipText(LOCTEXT("MarkUpToDateTip", "The doc is fine as it is. Records the date in its frontmatter (protoboy-reviewed) and takes it off this list; only changes made after now will flag it again."))
				.OnClicked_Lambda([this, DocId]()
				{
					FText Error;
					FProtoBoyDocHealthService* Health = GetHealth();
					// On success the service fires OnChanged, which rebuilds these cards.
					if (Health && !Health->MarkUpToDate(DocId, Error))
					{
						ResultStatus = Error;
					}
					return FReply::Handled();
				})
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

void SProtoBoyHealthPage::OpenDoc(const FString& DocId, const FString& Path, const FString& Title, FText& OutStatus)
{
	const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	if (!Subsystem)
	{
		return;
	}
	// A saved report may be older than the map: the doc may have moved or gone since.
	const FProtoBoyDocNode* Doc = Subsystem->GetDocMap().Find(DocId);
	if (!Doc)
	{
		Doc = Subsystem->GetDocMap().FindByPath(Path);
	}
	if (!Doc)
	{
		OutStatus = FText::Format(LOCTEXT("DocGone", "\"{0}\" isn't on the doc map any more."), FText::FromString(Title));
		return;
	}
	FText Error;
	OutStatus = Subsystem->GetDocsService().OpenDoc(Doc->Path, Error) ? FText::GetEmpty() : Error;
}

// --- Documentation structure ------------------------------------------------------------------------------------------

TSharedRef<SWidget> SProtoBoyHealthPage::MakeStructure()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("StructureDesc", "What the docs themselves show, without an agent: links to notes that don't exist, project paths that don't exist, sections nobody has written, Design docs with no Tech or Content doc, and parts of the project no doc mentions."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 8.f, 0.f, 6.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("CheckAgain", "Check Again"))
				.ToolTipText(LOCTEXT("CheckAgainTip", "Re-read the docs (to pick up edits made outside the editor) and check them again."))
				.OnClicked_Lambda([]()
				{
					if (UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get())
					{
						Subsystem->GetDocMap().Refresh();
						Subsystem->GetDocHealth().CheckStructure();
					}
					return FReply::Handled();
				})
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return GetStructureSummary(); })
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.AutoWrapText(true)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(IssueList, SVerticalBox)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Visibility_Lambda([this]() { return StructureStatus.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
			.Text_Lambda([this]() { return StructureStatus; })
			.ColorAndOpacity(FStyleColors::Error)
			.AutoWrapText(true)
		];
}

FText SProtoBoyHealthPage::GetStructureSummary() const
{
	const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	FText WhyNot;
	if (!Subsystem || !Subsystem->GetDocsProvider().IsReady(WhyNot))
	{
		return WhyNot.IsEmpty() ? LOCTEXT("StructureNoDocs", "The docs aren't available.") : WhyNot;
	}
	const FProtoBoyDocHealthService& Health = Subsystem->GetDocHealth();
	if (!Health.IsStructureChecked())
	{
		return LOCTEXT("StructureChecking", "Checking...");
	}
	const int32 NumDocs = Subsystem->GetDocMap().GetNodes().Num();
	const int32 NumIssues = Health.GetIssues().Num();
	if (NumIssues == 0)
	{
		return FText::Format(LOCTEXT("StructureClean", "Nothing found in {0} {0}|plural(one=doc,other=docs)."), NumDocs);
	}
	const int32 NumProblems = Health.CountProblems();
	return FText::Format(LOCTEXT("StructureSummary", "{0} {0}|plural(one=problem,other=problems) and {1} {1}|plural(one=thing,other=things) to fill in, across {2} {2}|plural(one=doc,other=docs)."),
		NumProblems, NumIssues - NumProblems, NumDocs);
}

void SProtoBoyHealthPage::ShowStructure()
{
	IssueList->ClearChildren();
	StructureStatus = FText::GetEmpty();
	const FProtoBoyDocHealthService* Health = GetHealth();
	if (!Health)
	{
		return;
	}
	for (const FProtoBoyDocIssue& Issue : Health->GetIssues())
	{
		IssueList->AddSlot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			MakeIssue(Issue)
		];
	}
}

TSharedRef<SWidget> SProtoBoyHealthPage::MakeIssue(const FProtoBoyDocIssue& Issue)
{
	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(Issue.DocId.IsEmpty() ? LOCTEXT("IssueAllDocs", "The docs as a whole") : FText::FromString(Issue.DocTitle))
			.ToolTipText(FText::FromString(Issue.DocPath))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
			.AutoWrapText(true)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(Issue.Message)
			.AutoWrapText(true)
		];

	if (!Issue.Details.IsEmpty())
	{
		Body->AddSlot()
		.AutoHeight()
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Join(Issue.Details, TEXT("\n"))))
			.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		];
	}

	const FString DocId = Issue.DocId;
	const FString DocPath = Issue.DocPath;
	const FString DocTitle = Issue.DocTitle;
	return SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		.Padding(8.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Top)
			.Padding(0.f, 1.f, 8.f, 0.f)
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush(Issue.bWarning ? "Icons.WarningWithColor" : "Icons.Info"))
				.ColorAndOpacity(Issue.bWarning ? FSlateColor(FLinearColor::White) : FSlateColor::UseSubduedForeground())
				.ToolTipText(Issue.bWarning ? LOCTEXT("IssueWarningTip", "Something the doc refers to isn't there.") : LOCTEXT("IssueNoteTip", "Something that hasn't been filled in yet."))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				Body
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Top)
			.Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.Visibility(DocId.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
				.Text(LOCTEXT("OpenIssueDoc", "Open Doc"))
				.ToolTipText(FText::Format(LOCTEXT("OpenIssueDocTip", "Open {0}."), FText::FromString(DocPath)))
				.OnClicked_Lambda([this, DocId, DocPath, DocTitle]() { OpenDoc(DocId, DocPath, DocTitle, StructureStatus); return FReply::Handled(); })
			]
		];
}

#undef LOCTEXT_NAMESPACE
