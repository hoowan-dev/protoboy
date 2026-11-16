#include "UI/Docs/Map/SPBSetupDocsDialog.h"

#include "Docs/Generation/PBDocGeneration.h"
#include "Docs/PBDocLayers.h"
#include "Docs/PBDocMap.h"
#include "Docs/PBDocTemplates.h"
#include "Framework/Application/SlateApplication.h"
#include "MCP/PBMCPClientConfig.h"
#include "Misc/MessageDialog.h"
#include "PBSubsystem.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "UI/Docs/Map/SPBDocMapNode.h"
#include "UI/Agents/SPBAgentPicker.h"
#include "UI/PBStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SProtoBoySetupDocsDialog"

namespace ProtoBoySetupDocsDialog
{
	FProtoBoyDocMap* GetMap()
	{
		UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
		return Subsystem ? &Subsystem->GetDocMap() : nullptr;
	}

	/** A small colored chip with the layer's name, so rows read like the map's nodes. */
	TSharedRef<SWidget> MakeKindChip(EProtoBoyDocKind Kind)
	{
		return SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.White"))
			.BorderBackgroundColor(SProtoBoyDocMapNode::GetLayerColor(ProtoBoyDocLayers::GetLayer(Kind)))
			.Padding(FMargin(6.f, 1.f))
			[
				SNew(SBox)
				.MinDesiredWidth(62.f)
				[
					SNew(STextBlock)
					.Text(ProtoBoyDocLayers::GetKindName(Kind))
					.TextStyle(FAppStyle::Get(), "SmallText")
					.ColorAndOpacity(FLinearColor::White)
				]
			];
	}

	/** One row of a doc tree: indent, layer chip, title, then an optional detail line in subdued text. */
	TSharedRef<SWidget> MakeDocRow(int32 Depth, EProtoBoyDocKind Kind, const FText& Title, const FText& Detail, bool bDimmed = false)
	{
		return SNew(SHorizontalBox)
			.ToolTipText(Detail)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(Depth * 18.f, 2.f, 8.f, 2.f)
			[
				MakeKindChip(Kind)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Title)
				.ColorAndOpacity(bDimmed ? FSlateColor::UseSubduedForeground() : FSlateColor::UseForeground())
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			.Padding(10.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(Detail)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			];
	}

	TSharedRef<SWidget> MakeHeading(const FText& Text)
	{
		return SNew(STextBlock)
			.Text(Text)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10));
	}
}

void SProtoBoySetupDocsDialog::Construct(const FArguments& InArgs)
{
	ParentWindow = InArgs._ParentWindow;
	SelectedAgent = SProtoBoyAgentPicker::GetDefaultAgent();
	for (FProtoBoyDocTemplate& Template : ProtoBoyDocTemplates::LoadTemplates())
	{
		Templates.Add(MakeShared<FProtoBoyDocTemplate>(MoveTemp(Template)));
	}

	ChildSlot
	[
		SNew(SBox)
		.Padding(16.f)
		[
			SAssignNew(StepSwitcher, SWidgetSwitcher)
			+ SWidgetSwitcher::Slot() [ MakeStartStep() ]        // EStep::Start
			+ SWidgetSwitcher::Slot() [ MakeChooseStep() ]       // EStep::Choose
			+ SWidgetSwitcher::Slot() [ MakeConfirmStep() ]      // EStep::Confirm
			+ SWidgetSwitcher::Slot() [ MakeFromProjectStep() ]  // EStep::FromProject
			+ SWidgetSwitcher::Slot() [ MakeGeneratingStep() ]   // EStep::Generating
		]
	];

	ShowStep(EStep::Start);
	if (Templates.Num() > 0)
	{
		TemplateList->SetSelection(Templates[0]);
	}
}

SProtoBoySetupDocsDialog::~SProtoBoySetupDocsDialog()
{
	// Closing mid-run (e.g. the editor shutting down) stops the agent; the run's destructor ends the process.
}

void SProtoBoySetupDocsDialog::ShowStep(EStep Step)
{
	CurrentStep = Step;
	if (Step == EStep::Confirm && SelectedTemplate.IsValid())
	{
		ApplyError = FText::GetEmpty();
		PlanBox->SetContent(MakePlan(*SelectedTemplate));
	}
	StepSwitcher->SetActiveWidgetIndex(static_cast<int32>(Step));
}

// --- Start --------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> SProtoBoySetupDocsDialog::MakeStartStep()
{
	const TSharedRef<SWidget> Body = SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 12.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("StartIntro", "How do you want to start the project's docs?"))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			MakeStartOption("Icons.Documentation",
				LOCTEXT("StartTemplate", "Start from a template"),
				LOCTEXT("StartTemplateDesc", "Pick a preset for your kind of game (by genre or by size): the Game doc with suggested design pillars, and a starting set of Design, Tech and Content docs."),
				EStep::Choose)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 8.f, 0.f, 0.f)
		[
			MakeStartOption("Icons.Search",
				LOCTEXT("StartProject", "Generate from the Project (EXPERIMENTAL)"),
				LOCTEXT("StartProjectDesc", "An agent reads what's already in the game (code, Blueprints, assets and levels) and writes the docs to match what exists."),
				EStep::FromProject)
		];

	return MakeStepLayout(LOCTEXT("StartTitle", "Set Up Docs from Template"), Body, TOptional<EStep>(), SNullWidget::NullWidget);
}

TSharedRef<SWidget> SProtoBoySetupDocsDialog::MakeStartOption(const FName& Icon, const FText& Title, const FText& Description, EStep Step)
{
	return SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.ContentPadding(FMargin(12.f, 10.f))
		.OnClicked_Lambda([this, Step]()
		{
			ShowStep(Step);
			return FReply::Handled();
		})
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
			.Padding(12.f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 12.f, 0.f)
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush(Icon))
					.DesiredSizeOverride(FVector2D(24.f, 24.f))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(Title)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 2.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Text(Description)
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.AutoWrapText(true)
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(12.f, 0.f, 0.f, 0.f)
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.ChevronRight"))
				]
			]
		];
}

// --- From the project ---------------------------------------------------------------------------------------------

TSharedRef<SWidget> SProtoBoySetupDocsDialog::MakeFromProjectStep()
{
	auto Paragraph = [](const FText& Text)
	{
		return SNew(STextBlock).Text(Text).AutoWrapText(true);
	};

	const TSharedRef<SWidget> Body = SNew(SScrollBox)

		+ SScrollBox::Slot()
		.Padding(0.f, 0.f, 0.f, 8.f)
		[
			Paragraph(LOCTEXT("FromProjectWhat", "One of your agents reads what's already in the project (the .uproject, config, C++ source and plugins, plus a summary of the Content folder's maps, Blueprints and data assets that ProtoBoy gives it) and drafts the docs from it: the Game doc with its pitch, core loop and design pillars, and Design, Tech and Content docs for the features, systems and assets it finds, nested on the doc map."))
		]

		+ SScrollBox::Slot()
		.Padding(0.f, 0.f, 0.f, 8.f)
		[
			Paragraph(LOCTEXT("FromProjectHow", "The agent runs on your machine with your own account, reads files only and changes nothing in the project. ProtoBoy writes the docs it returns. You'll see its progress as it works; it can take several minutes."))
		]

		+ SScrollBox::Slot()
		.Padding(0.f, 6.f, 0.f, 4.f)
		[
			ProtoBoySetupDocsDialog::MakeHeading(LOCTEXT("AgentHeading", "Agent"))
		]

		+ SScrollBox::Slot()
		[
			SNew(SProtoBoyAgentPicker)
			.SelectedAgent_Lambda([this]() { return SelectedAgent; })
			.OnAgentPicked_Lambda([this](EProtoBoyMCPClient Agent) { SelectedAgent = Agent; })
		]

		+ SScrollBox::Slot()
		.Padding(0.f, 8.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Visibility_Lambda([this]() { return GenerateError.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
			.Text_Lambda([this]() { return GenerateError; })
			.ColorAndOpacity(FStyleColors::Error)
			.AutoWrapText(true)
		];

	const TSharedRef<SWidget> Generate = SNew(SButton)
		.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
		.Text(LOCTEXT("Generate", "Generate"))
		.ToolTipText(LOCTEXT("GenerateTip", "Replaces your docs with ones the agent drafts from the project. Asks first."))
		.IsEnabled_Lambda([this]()
		{
			FText Ignored;
			return SelectedAgent.IsSet() && FProtoBoyDocGenerationRun::CanRun(SelectedAgent.GetValue(), Ignored);
		})
		.OnClicked(this, &SProtoBoySetupDocsDialog::OnGenerateClicked);

	return MakeStepLayout(LOCTEXT("FromProjectTitle", "Generate Docs from the Project (EXPERIMENTAL)"), Body, EStep::Start, Generate);
}

FReply SProtoBoySetupDocsDialog::OnGenerateClicked()
{
	FProtoBoyDocMap* Map = ProtoBoySetupDocsDialog::GetMap();
	UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	if (!Map || !Subsystem || !SelectedAgent.IsSet())
	{
		return FReply::Handled();
	}

	const FText Warning = FText::Format(LOCTEXT("GenerateWarning", "This overwrites all of your notes: every doc on the doc map, the Game doc included, moves to {0} before the agent starts.\n\nThe docs it writes are a good starting point, but they only describe what's in the project so far. It's recommended that you go through them afterwards and add the specific details of your game.\n\nGenerate the docs with {1}?"),
		Subsystem->GetDocsProvider().GetDeletedDocsLocation(), FProtoBoyMCPClientConfig::GetClientName(SelectedAgent.GetValue()));
	if (FMessageDialog::Open(EAppMsgType::YesNo, EAppReturnType::No, Warning, LOCTEXT("GenerateWarningTitle", "Overwrite Docs")) != EAppReturnType::Yes)
	{
		return FReply::Handled();
	}

	Run = MakeUnique<FProtoBoyDocGenerationRun>(*Map);
	LogShown = 0;
	LogScroll->ClearChildren();
	GenerateError = FText::GetEmpty();
	if (!Run->Start(SelectedAgent.GetValue(), GenerateError))
	{
		Run.Reset();
		return FReply::Handled();
	}
	ShowStep(EStep::Generating);
	PollGeneration();
	RegisterActiveTimer(0.1f, FWidgetActiveTimerDelegate::CreateLambda([this](double, float)
	{
		PollGeneration();
		return Run.IsValid() && !Run->IsFinished() ? EActiveTimerReturnType::Continue : EActiveTimerReturnType::Stop;
	}));
	return FReply::Handled();
}

TSharedRef<SWidget> SProtoBoySetupDocsDialog::MakeGeneratingStep()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("GeneratingTitle", "Generating Docs"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
		]

		// Status: what's happening, with a throbber while the agent runs
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
				SNew(SCircularThrobber)
				.Radius(8.f)
				.Visibility_Lambda([this]() { return Run.IsValid() && Run->IsRunning() ? EVisibility::Visible : EVisibility::Collapsed; })
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text_Lambda([this]()
				{
					if (!Run.IsValid() || Run->IsRunning())
					{
						return FText::Format(LOCTEXT("StatusRunning", "{0} is reading the project and drafting the docs..."),
							SelectedAgent.IsSet() ? FProtoBoyMCPClientConfig::GetClientName(SelectedAgent.GetValue()) : FText::GetEmpty());
					}
					return Run->Succeeded()
						? LOCTEXT("StatusDone", "Done. The docs are on the doc map: go through them and add the details of your game.")
						: LOCTEXT("StatusFailed", "The agent didn't finish, so no docs were written. See the log for why.");
				})
				.ColorAndOpacity_Lambda([this]()
				{
					return Run.IsValid() && Run->IsFinished() && !Run->Succeeded() ? FSlateColor(FStyleColors::Error) : FSlateColor::UseForeground();
				})
			]
		]

		// While the agent runs: closing early cancels (red). Once it's done: complete (green). A failed run has no line
		// here; the status above says what went wrong.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 8.f)
		[
			SNew(STextBlock)
			.Visibility_Lambda([this]()
			{
				return Run.IsValid() && (Run->IsRunning() || Run->Succeeded()) ? EVisibility::Visible : EVisibility::Collapsed;
			})
			.Text_Lambda([this]()
			{
				return Run.IsValid() && Run->Succeeded()
					? LOCTEXT("GenerationComplete", "Doc generation is complete. Click Finish to close this window.")
					: LOCTEXT("ExitCancels", "Exiting this window before the agent completes will cancel doc generation.");
			})
			.ColorAndOpacity_Lambda([this]()
			{
				return Run.IsValid() && Run->Succeeded() ? FSlateColor(FStyleColors::Success) : FSlateColor(FStyleColors::Error);
			})
			.AutoWrapText(true)
		]

		// Agent log: its own scrollable panel, so a long run never pushes the buttons off the window
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 4.f)
		[
			ProtoBoySetupDocsDialog::MakeHeading(LOCTEXT("AgentLogHeading", "Agent log"))
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
			.Padding(8.f)
			[
				SAssignNew(LogScroll, SScrollBox)
				.Orientation(Orient_Vertical)
				.ScrollBarAlwaysVisible(true)
			]
		]

		// Cancel on the left, Finish on the right
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 12.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("CancelGenerating", "Cancel"))
				.ToolTipText(LOCTEXT("CancelGeneratingTip", "Stop the agent and clear the project's docs. Asks first."))
				.OnClicked_Lambda([this]()
				{
					if (ConfirmCancelGeneration())
					{
						if (Run.IsValid())
						{
							Run->Cancel();
						}
						CloseWindow();
					}
					return FReply::Handled();
				})
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNullWidget::NullWidget
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
				.Text(LOCTEXT("Finish", "Finish"))
				.ToolTipText(LOCTEXT("FinishTip", "Available once the agent is done."))
				.IsEnabled_Lambda([this]() { return Run.IsValid() && Run->IsFinished(); })
				.OnClicked_Lambda([this]()
				{
					CloseWindow();
					return FReply::Handled();
				})
			]
		];
}

void SProtoBoySetupDocsDialog::PollGeneration()
{
	if (!Run.IsValid() || !LogScroll.IsValid())
	{
		return;
	}
	Run->Tick();
	const TArray<FString>& Log = Run->GetLog();
	if (LogShown >= Log.Num())
	{
		return;
	}
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

bool SProtoBoySetupDocsDialog::ConfirmCancelGeneration() const
{
	return FMessageDialog::Open(EAppMsgType::YesNo, EAppReturnType::No,
		LOCTEXT("CancelGeneratingConfirm", "Cancel generating the docs?\n\nThe agent stops and the project's docs are cleared, including any it has written. Your previous docs are in the trash."),
		LOCTEXT("CancelGeneratingTitle", "Cancel Generating Docs")) == EAppReturnType::Yes;
}

void SProtoBoySetupDocsDialog::OnWindowCloseRequested(const TSharedRef<SWindow>& Window)
{
	// While generating, the close button is Cancel: ask, stop the agent and clear the docs.
	if (!bClosing && CurrentStep == EStep::Generating)
	{
		if (!ConfirmCancelGeneration())
		{
			return;
		}
		if (Run.IsValid())
		{
			Run->Cancel();
		}
	}
	FSlateApplication::Get().RequestDestroyWindow(Window);
}

void SProtoBoySetupDocsDialog::CloseWindow()
{
	bClosing = true;
	if (const TSharedPtr<SWindow> Window = ParentWindow.Pin())
	{
		Window->RequestDestroyWindow();
	}
}

// --- Choose -------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> SProtoBoySetupDocsDialog::MakeChooseStep()
{
	const TSharedRef<SWidget> Body = SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 12.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ChooseIntro", "Start the project's docs from a preset for your kind of game: the Game doc with suggested design pillars, and Design, Tech and Content docs from their templates. Nothing you've already written is overwritten, and you can add, move or delete docs afterwards."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SHorizontalBox)

			// Template list
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 12.f, 0.f)
			[
				SNew(SBox)
				.WidthOverride(220.f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
					.Padding(4.f)
					[
						SAssignNew(TemplateList, SListView<TSharedPtr<FProtoBoyDocTemplate>>)
						.ListItemsSource(&Templates)
						.SelectionMode(ESelectionMode::Single)
						.OnGenerateRow(this, &SProtoBoySetupDocsDialog::OnGenerateTemplateRow)
						.OnSelectionChanged(this, &SProtoBoySetupDocsDialog::OnTemplateSelected)
					]
				]
			]

			// Selected template
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
				.Padding(10.f)
				[
					SAssignNew(PreviewBox, SBox)
					[
						SNew(STextBlock)
						.Text(Templates.IsEmpty()
							? FText::Format(LOCTEXT("NoTemplates", "No templates found in {0}."), FText::FromString(ProtoBoyDocTemplates::GetTemplatesDir()))
							: LOCTEXT("PickTemplate", "Select a template to preview it."))
						.AutoWrapText(true)
					]
				]
			]
		];

	const TSharedRef<SWidget> Next = SNew(SButton)
		.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
		.Text(LOCTEXT("Next", "Next"))
		.ToolTipText(LOCTEXT("NextTip", "See exactly which docs the template creates before anything is written."))
		.IsEnabled_Lambda([this]() { return SelectedTemplate.IsValid(); })
		.OnClicked_Lambda([this]()
		{
			ShowStep(EStep::Confirm);
			return FReply::Handled();
		});

	return MakeStepLayout(LOCTEXT("ChooseTitle", "Choose a Template"), Body, EStep::Start, Next);
}

TSharedRef<ITableRow> SProtoBoySetupDocsDialog::OnGenerateTemplateRow(TSharedPtr<FProtoBoyDocTemplate> Template, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FProtoBoyDocTemplate>>, OwnerTable)
		.Padding(FMargin(8.f, 6.f))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Template->Name))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 2.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("RowSubtitle", "{0} · {1} docs"),
					Template->Genre.IsEmpty() ? LOCTEXT("AnyGenre", "Any genre") : FText::FromString(Template->Genre),
					FText::AsNumber(Template->Docs.Num())))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		];
}

void SProtoBoySetupDocsDialog::OnTemplateSelected(TSharedPtr<FProtoBoyDocTemplate> Template, ESelectInfo::Type SelectInfo)
{
	// Keep a selection: clicking empty space in the list would otherwise blank the preview.
	if (!Template.IsValid())
	{
		if (SelectedTemplate.IsValid())
		{
			TemplateList->SetSelection(SelectedTemplate);
		}
		return;
	}
	SelectedTemplate = Template;
	PreviewBox->SetContent(MakePreview(*Template));
}

TSharedRef<SWidget> SProtoBoySetupDocsDialog::MakePreview(const FProtoBoyDocTemplate& Template)
{
	TSharedRef<SScrollBox> Scroll = SNew(SScrollBox);

	Scroll->AddSlot()
	[
		SNew(STextBlock)
		.Text(FText::FromString(Template.Name))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
	];
	Scroll->AddSlot()
	.Padding(0.f, 2.f, 0.f, 10.f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(Template.Description))
		.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		.AutoWrapText(true)
	];

	// Design pillars, as the Game doc will list them
	Scroll->AddSlot()[ ProtoBoySetupDocsDialog::MakeHeading(LOCTEXT("PillarsHeading", "Suggested design pillars")) ];
	Scroll->AddSlot()
	.Padding(0.f, 2.f, 0.f, 4.f)
	[
		SNew(STextBlock)
		.Text(Template.Pillars.IsEmpty()
			? LOCTEXT("PillarsNoneNote", "None: the Game doc starts with one example pillar for you to replace with your own. Every feature should serve one.")
			: LOCTEXT("PillarsNote", "Written into a new Game doc as placeholders. Rewrite them for your game: every feature should serve one."))
		.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		.AutoWrapText(true)
	];
	for (const FProtoBoyDocTemplatePillar& Pillar : Template.Pillars)
	{
		Scroll->AddSlot()
		.Padding(8.f, 3.f, 0.f, 0.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(FText::FromString(Pillar.Name)).Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("PillarLine", "Feeling: {0}. Rules out: {1}."), FText::FromString(Pillar.Feeling), FText::FromString(Pillar.RulesOut)))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.AutoWrapText(true)
			]
		];
	}

	// The docs, nested the way the map will nest them
	Scroll->AddSlot()
	.Padding(0.f, 14.f, 0.f, 4.f)
	[
		ProtoBoySetupDocsDialog::MakeHeading(FText::Format(LOCTEXT("DocsHeading", "Docs ({0})"), FText::AsNumber(Template.Docs.Num() + 1)))
	];
	Scroll->AddSlot()[ ProtoBoySetupDocsDialog::MakeDocRow(0, EProtoBoyDocKind::Game, FText::FromString(FProtoBoyDocMap::GetProjectTitle()), LOCTEXT("GameDetail", "The Game doc: pitch, the design pillars above, non-goals, core loop, and an index of everything below")) ];

	TMap<FString, int32> Depths;
	for (const FProtoBoyDocTemplateDoc& Doc : Template.Docs)
	{
		EProtoBoyDocKind Kind = EProtoBoyDocKind::Design;
		ProtoBoyDocLayers::FromTemplateKey(Doc.Kind, Kind);
		const int32 Depth = Doc.Parent.IsEmpty() ? 0 : Depths.FindRef(Doc.Parent) + 1;
		Depths.Add(Doc.Id, Depth);
		Scroll->AddSlot()[ ProtoBoySetupDocsDialog::MakeDocRow(Depth + 1, Kind, FText::FromString(Doc.Title), FText::FromString(Doc.Summary)) ];
	}
	return Scroll;
}

// --- Confirm ------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> SProtoBoySetupDocsDialog::MakeConfirmStep()
{
	const TSharedRef<SWidget> Body = SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
			.Padding(10.f)
			[
				SAssignNew(PlanBox, SBox)
			]
		]

		// What goes in the new docs' sections. Hidden for templates without examples (Blank).
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 10.f, 0.f, 0.f)
		[
			SNew(SVerticalBox)
			.Visibility_Lambda([this]()
			{
				return SelectedTemplate.IsValid() && ProtoBoyDocTemplates::HasExamples(*SelectedTemplate) ? EVisibility::Visible : EVisibility::Collapsed;
			})

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				ProtoBoySetupDocsDialog::MakeHeading(LOCTEXT("ContentsHeading", "Note contents"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "RadioButton")
				.IsChecked_Lambda([this]() { return bWriteExamples ? ECheckBoxState::Unchecked : ECheckBoxState::Checked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState) { bWriteExamples = false; })
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ContentsBlank", "Leave blank: each doc gets its headings, the hints under them, and a line on what it covers."))
					.AutoWrapText(true)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "RadioButton")
				.IsChecked_Lambda([this]() { return bWriteExamples ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState) { bWriteExamples = true; })
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ContentsExamples", "Add example documentation: the Game doc and each doc's sections start with example content for this kind of game (what the player can do, how it works, the assets it needs), marked as examples to rewrite."))
					.AutoWrapText(true)
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 8.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Visibility_Lambda([this]() { return ApplyError.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
			.Text_Lambda([this]() { return ApplyError; })
			.ColorAndOpacity(FStyleColors::Error)
			.AutoWrapText(true)
		];

	const TSharedRef<SWidget> Apply = SNew(SButton)
		.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
		.Text_Lambda([this]()
		{
			return PlannedCount > 0
				? FText::Format(PlannedRemoved > 0
					? LOCTEXT("ReplaceDocs", "Replace with {0} {0}|plural(one=Doc,other=Docs)")
					: LOCTEXT("CreateDocs", "Create {0} {0}|plural(one=Doc,other=Docs)"), FText::AsNumber(PlannedCount))
				: LOCTEXT("NothingToCreate", "Nothing to Create");
		})
		.IsEnabled_Lambda([this]() { return SelectedTemplate.IsValid() && PlannedCount > 0; })
		.OnClicked(this, &SProtoBoySetupDocsDialog::OnApplyClicked);

	return MakeStepLayout(LOCTEXT("ConfirmTitle", "Confirm Template"), Body, EStep::Choose, Apply);
}

TSharedRef<SWidget> SProtoBoySetupDocsDialog::MakePlan(const FProtoBoyDocTemplate& Template)
{
	const FProtoBoyDocMap* Map = ProtoBoySetupDocsDialog::GetMap();
	if (!Map)
	{
		PlannedCount = 0;
		PlannedRemoved = 0;
		return SNew(STextBlock).Text(LOCTEXT("NoMap", "ProtoBoy isn't running."));
	}
	const FProtoBoyDocTemplatePlan Plan = Map->PlanTemplate(Template);
	PlannedCount = Plan.NumToCreate;
	PlannedRemoved = Plan.Removed.Num();
	const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	const FText Trash = Subsystem ? Subsystem->GetDocsProvider().GetDeletedDocsLocation() : FText::GetEmpty();

	TSharedRef<SScrollBox> Scroll = SNew(SScrollBox);
	if (!Plan.Removed.IsEmpty())
	{
		// Applying a template replaces the documentation, so say so before anything else.
		Scroll->AddSlot()
		[
			SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("PlanReplace", "This replaces your documentation. {0} existing {0}|plural(one=doc moves,other=docs move) to {1} first, the Game doc included:"),
				FText::AsNumber(Plan.Removed.Num()), Trash))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
			.ColorAndOpacity(FStyleColors::Warning)
			.AutoWrapText(true)
		];
		for (const FString& Path : Plan.Removed)
		{
			Scroll->AddSlot()
			.Padding(16.f, 1.f, 0.f, 1.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Path))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
		}
		Scroll->AddSlot()
		.Padding(0.f, 2.f, 0.f, 12.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PlanReplaceKeep", "Folders they leave empty are removed too. Other notes in the docs folder aren't touched, and their folders stay. Copy anything you want to keep out of the trash afterwards."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		];
	}
	Scroll->AddSlot()
	[
		SNew(STextBlock)
		.Text(FText::Format(LOCTEXT("PlanSummary", "{0}: {1} {1}|plural(one=doc,other=docs) will be written."),
			FText::FromString(Template.Name), FText::AsNumber(Plan.NumToCreate)))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		.AutoWrapText(true)
	];
	Scroll->AddSlot()
	.Padding(0.f, 2.f, 0.f, 10.f)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("PlanGameNew", "The Game doc is created with the template's suggested design pillars. New docs start from their template's sections, with a line on what each covers."))
		.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		.AutoWrapText(true)
	];

	Scroll->AddSlot()[ ProtoBoySetupDocsDialog::MakeDocRow(0, EProtoBoyDocKind::Game, FText::FromString(FProtoBoyDocMap::GetProjectTitle()), FText::FromString(FProtoBoyDocMap::GetGameDocPath())) ];
	for (const FProtoBoyDocTemplatePlan::FEntry& Entry : Plan.Docs)
	{
		Scroll->AddSlot()[ ProtoBoySetupDocsDialog::MakeDocRow(Entry.Depth + 1, Entry.Kind, FText::FromString(Entry.Title), FText::FromString(Entry.Path)) ];
	}
	return Scroll;
}

FReply SProtoBoySetupDocsDialog::OnApplyClicked()
{
	FProtoBoyDocMap* Map = ProtoBoySetupDocsDialog::GetMap();
	if (!Map || !SelectedTemplate.IsValid())
	{
		return FReply::Handled();
	}
	int32 Created = 0;
	const bool bExamples = bWriteExamples && ProtoBoyDocTemplates::HasExamples(*SelectedTemplate);
	if (!Map->ApplyTemplate(*SelectedTemplate, bExamples, Created, ApplyError))
	{
		// Some docs may have been written before the failure; show what's there now.
		PlanBox->SetContent(MakePlan(*SelectedTemplate));
		return FReply::Handled();
	}
	if (const TSharedPtr<SWindow> Window = ParentWindow.Pin())
	{
		Window->RequestDestroyWindow();
	}
	return FReply::Handled();
}

// --- Layout -------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> SProtoBoySetupDocsDialog::MakeStepLayout(const FText& Title, const TSharedRef<SWidget>& Body, TOptional<EStep> BackTo, const TSharedRef<SWidget>& Action)
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(Title)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		.Padding(0.f, 12.f)
		[
			Body
		]

		// Back (or Cancel on the first step) on the left, the step's action on the right
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(BackTo.IsSet() ? LOCTEXT("Back", "Back") : LOCTEXT("Cancel", "Cancel"))
				.OnClicked_Lambda([this, BackTo]()
				{
					if (BackTo.IsSet())
					{
						ShowStep(BackTo.GetValue());
					}
					else if (const TSharedPtr<SWindow> Window = ParentWindow.Pin())
					{
						Window->RequestDestroyWindow();
					}
					return FReply::Handled();
				})
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNullWidget::NullWidget
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				Action
			]
		];
}

void SProtoBoySetupDocsDialog::Open(const TSharedPtr<SWidget>& ParentWidget)
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Set Up Docs from Template"))
		.ClientSize(FVector2D(1000.f, 640.f))
		.SupportsMinimize(false)
		.SupportsMaximize(false);

	const TSharedRef<SProtoBoySetupDocsDialog> Dialog = SNew(SProtoBoySetupDocsDialog).ParentWindow(Window);
	Window->SetContent(Dialog);
	Window->SetRequestDestroyWindowOverride(FRequestDestroyWindowOverride::CreateSP(Dialog, &SProtoBoySetupDocsDialog::OnWindowCloseRequested));
	FSlateApplication::Get().AddModalWindow(Window, ParentWidget);
}

#undef LOCTEXT_NAMESPACE
