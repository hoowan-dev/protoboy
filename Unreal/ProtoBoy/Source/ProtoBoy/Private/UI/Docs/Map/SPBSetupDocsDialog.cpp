#include "UI/Docs/Map/SPBSetupDocsDialog.h"

#include "Docs/PBDocLayers.h"
#include "Docs/PBDocMap.h"
#include "Docs/PBDocTemplates.h"
#include "Framework/Application/SlateApplication.h"
#include "PBSubsystem.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "UI/Docs/Map/SPBDocMapNode.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
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
			+ SWidgetSwitcher::Slot() [ MakeChooseStep() ]   // EStep::Choose
			+ SWidgetSwitcher::Slot() [ MakeConfirmStep() ]  // EStep::Confirm
		]
	];

	ShowStep(EStep::Choose);
	if (Templates.Num() > 0)
	{
		TemplateList->SetSelection(Templates[0]);
	}
}

void SProtoBoySetupDocsDialog::ShowStep(EStep Step)
{
	if (Step == EStep::Confirm && SelectedTemplate.IsValid())
	{
		ApplyError = FText::GetEmpty();
		PlanBox->SetContent(MakePlan(*SelectedTemplate));
	}
	StepSwitcher->SetActiveWidgetIndex(static_cast<int32>(Step));
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
			.Text(LOCTEXT("ChooseIntro", "Start the project's docs from a preset for your kind of game: the Game doc with suggested design pillars, and Design, Tech, Content and style guide docs from their templates. Nothing you've already written is overwritten, and you can add, move or delete docs afterwards."))
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

	return MakeStepLayout(LOCTEXT("ChooseTitle", "Set Up Docs from Template"), Body, TOptional<EStep>(), Next);
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
		.Text(LOCTEXT("PillarsNote", "Written into a new Game doc as placeholders. Rewrite them for your game: every feature should serve one."))
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
	Scroll->AddSlot()[ ProtoBoySetupDocsDialog::MakeDocRow(0, EProtoBoyDocKind::Game, LOCTEXT("GameTitle", "Game"), LOCTEXT("GameDetail", "Pitch, the design pillars above, non-goals, core loop, and an index of everything below")) ];

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
				? FText::Format(LOCTEXT("CreateDocs", "Create {0} {0}|plural(one=Doc,other=Docs)"), FText::AsNumber(PlannedCount))
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
		return SNew(STextBlock).Text(LOCTEXT("NoMap", "ProtoBoy isn't running."));
	}
	const FProtoBoyDocTemplatePlan Plan = Map->PlanTemplate(Template);
	PlannedCount = Plan.NumToCreate;
	const int32 Kept = Plan.Docs.FilterByPredicate([](const FProtoBoyDocTemplatePlan::FEntry& Entry) { return Entry.bExists; }).Num()
		+ (Plan.bCreatesGame ? 0 : 1);

	TSharedRef<SScrollBox> Scroll = SNew(SScrollBox);
	Scroll->AddSlot()
	[
		SNew(STextBlock)
		.Text(FText::Format(LOCTEXT("PlanSummary", "{0}: {1} new {1}|plural(one=doc,other=docs) will be written. {2} {2}|plural(one=doc is,other=docs are) already there and won't be changed."),
			FText::FromString(Template.Name), FText::AsNumber(Plan.NumToCreate), FText::AsNumber(Kept)))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		.AutoWrapText(true)
	];
	Scroll->AddSlot()
	.Padding(0.f, 2.f, 0.f, 10.f)
	[
		SNew(STextBlock)
		.Text(Plan.bCreatesGame
			? LOCTEXT("PlanGameNew", "The Game doc is created with the template's suggested design pillars. New docs start from their template's sections, with a line on what each covers.")
			: LOCTEXT("PlanGameKept", "Your Game doc is kept as it is, so the template's pillars aren't added; copy any you want from the previous step. New docs start from their template's sections."))
		.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		.AutoWrapText(true)
	];

	auto Status = [](bool bExists, const FString& Path)
	{
		return bExists
			? FText::Format(LOCTEXT("Keep", "Kept: {0} already exists"), FText::FromString(Path))
			: FText::Format(LOCTEXT("New", "New: {0}"), FText::FromString(Path));
	};
	const FProtoBoyDocNode* Game = Map->GetGame();
	Scroll->AddSlot()[ ProtoBoySetupDocsDialog::MakeDocRow(0, EProtoBoyDocKind::Game, LOCTEXT("PlanGame", "Game"), Status(!Plan.bCreatesGame, Game ? Game->Path : FString(TEXT("Game.md"))), !Plan.bCreatesGame) ];
	for (const FProtoBoyDocTemplatePlan::FEntry& Entry : Plan.Docs)
	{
		Scroll->AddSlot()[ ProtoBoySetupDocsDialog::MakeDocRow(Entry.Depth + 1, Entry.Kind, FText::FromString(Entry.Title), Status(Entry.bExists, Entry.Path), Entry.bExists) ];
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
	if (!Map->ApplyTemplate(*SelectedTemplate, Created, ApplyError))
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

	Window->SetContent(SNew(SProtoBoySetupDocsDialog).ParentWindow(Window));
	FSlateApplication::Get().AddModalWindow(Window, ParentWidget);
}

#undef LOCTEXT_NAMESPACE
