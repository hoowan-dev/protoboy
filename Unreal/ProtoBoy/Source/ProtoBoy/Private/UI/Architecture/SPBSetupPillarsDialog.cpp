#include "UI/Architecture/SPBSetupPillarsDialog.h"

#include "Architecture/PBPillarTemplates.h"
#include "Framework/Application/SlateApplication.h"
#include "PBSubsystem.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "UI/Architecture/SPBPillarTreePreview.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SProtoBoySetupPillarsDialog"

void SProtoBoySetupPillarsDialog::Construct(const FArguments& InArgs)
{
	ParentWindow = InArgs._ParentWindow;

	ChildSlot
	[
		SNew(SBox)
		.Padding(16.f)
		[
			SAssignNew(StepSwitcher, SWidgetSwitcher)
			+ SWidgetSwitcher::Slot() [ MakeChooseStep() ]               // EStep::Choose
			+ SWidgetSwitcher::Slot() [ MakeFromTemplateStep() ]         // EStep::FromTemplate
			+ SWidgetSwitcher::Slot() [ MakeFromExistingProjectStep() ]  // EStep::FromExistingProject
			+ SWidgetSwitcher::Slot() [ MakeConfirmTemplateStep() ]      // EStep::ConfirmTemplate
		]
	];

	ShowStep(EStep::Choose);
}

void SProtoBoySetupPillarsDialog::ShowStep(EStep Step)
{
	StepSwitcher->SetActiveWidgetIndex(static_cast<int32>(Step));
}

TSharedRef<SWidget> SProtoBoySetupPillarsDialog::MakeChooseStep()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ChooseTitle", "How do you want to build your pillars?"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 4.f, 0.f, 16.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ChooseDescription", "Start from a preset for your kind of game, or let an agent propose pillars from what you've already built. You can edit the tree afterwards either way."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			MakeOption(
				LOCTEXT("FromTemplateOption", "Build From Template"),
				LOCTEXT("FromTemplateOptionTip", "Pick a template based on preset game genres and scopes."),
				"Icons.Layout",
				EStep::FromTemplate)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 8.f, 0.f, 0.f)
		[
			MakeOption(
				LOCTEXT("FromExistingOption", "Build From Existing Project"),
				LOCTEXT("FromExistingOptionTip", "Use the work you've done on your project so far, and have an LLM build pillars around it."),
				"Icons.Search",
				EStep::FromExistingProject)
		];
}

TSharedRef<SWidget> SProtoBoySetupPillarsDialog::MakeOption(const FText& Title, const FText& Description, FName Icon, EStep Target)
{
	return SNew(SButton)
		.ContentPadding(FMargin(12.f, 10.f))
		.OnClicked_Lambda([this, Target]()
		{
			ShowStep(Target);
			return FReply::Handled();
		})
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
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		];
}

TSharedRef<SWidget> SProtoBoySetupPillarsDialog::MakeFromTemplateStep()
{
	// TODO: Filter by genre and scope once there are more templates.
	for (FProtoBoyPillarTemplate& Template : ProtoBoyPillarTemplates::LoadTemplates())
	{
		Templates.Add(MakeShared<FProtoBoyPillarTemplate>(MoveTemp(Template)));
	}

	const TSharedRef<SWidget> Body = SNew(SHorizontalBox)

		// Template list (scrolls on its own)
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
					SAssignNew(TemplateList, SListView<TSharedPtr<FProtoBoyPillarTemplate>>)
					.ListItemsSource(&Templates)
					.SelectionMode(ESelectionMode::Single)
					.OnGenerateRow(this, &SProtoBoySetupPillarsDialog::OnGenerateTemplateRow)
					.OnSelectionChanged(this, &SProtoBoySetupPillarsDialog::OnTemplateSelected)
				]
			]
		]

		// Selected template: description, then its tree
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 8.f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					if (Templates.IsEmpty())
					{
						return FText::Format(LOCTEXT("NoTemplates", "No templates found in {0}."), FText::FromString(ProtoBoyPillarTemplates::GetTemplatesDir()));
					}
					return SelectedTemplate.IsValid() ? FText::FromString(SelectedTemplate->Description) : LOCTEXT("NoTemplate", "Select a template to preview its pillars.");
				})
				.AutoWrapText(true)
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SAssignNew(TemplatePreview, SProtoBoyPillarTreePreview)
			]
		];

	if (Templates.Num() > 0)
	{
		TemplateList->SetSelection(Templates[0]);
	}

	const TSharedRef<SWidget> Next = SNew(SButton)
		.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
		.Text(LOCTEXT("Next", "Next"))
		.ToolTipText(LOCTEXT("NextTip", "Review the selected template before it replaces your pillars."))
		.IsEnabled_Lambda([this]() { return SelectedTemplate.IsValid(); })
		.OnClicked_Lambda([this]()
		{
			ShowStep(EStep::ConfirmTemplate);
			ConfirmPreview->SetPillars(SelectedTemplate->Pillars);
			return FReply::Handled();
		});

	return MakeSubStep(LOCTEXT("FromTemplateTitle", "Build From Template"), Body, EStep::Choose, Next);
}

TSharedRef<ITableRow> SProtoBoySetupPillarsDialog::OnGenerateTemplateRow(TSharedPtr<FProtoBoyPillarTemplate> Template, const TSharedRef<STableViewBase>& OwnerTable)
{
	// "Genre · Scope" under the name, when the template is meant for a particular kind of game.
	TArray<FText> Tags;
	if (!Template->Genre.IsEmpty())
	{
		Tags.Add(FText::FromString(Template->Genre));
	}
	if (!Template->Scope.IsEmpty())
	{
		Tags.Add(FText::FromString(Template->Scope));
	}
	const FText Subtitle = Tags.IsEmpty() ? LOCTEXT("AnyGame", "Any genre or scope") : FText::Join(LOCTEXT("TagSeparator", " · "), Tags);

	return SNew(STableRow<TSharedPtr<FProtoBoyPillarTemplate>>, OwnerTable)
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
				.Text(Subtitle)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		];
}

void SProtoBoySetupPillarsDialog::OnTemplateSelected(TSharedPtr<FProtoBoyPillarTemplate> Template, ESelectInfo::Type SelectInfo)
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
	if (TemplatePreview.IsValid())
	{
		TemplatePreview->SetPillars(Template->Pillars);
	}
}

TSharedRef<SWidget> SProtoBoySetupPillarsDialog::MakeConfirmTemplateStep()
{
	auto HasPillars = []()
	{
		const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
		return Subsystem && Subsystem->GetPillars().HasPillars();
	};

	const TSharedRef<SWidget> Body = SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text_Lambda([this]()
			{
				return SelectedTemplate.IsValid()
					? FText::Format(LOCTEXT("ConfirmSelected", "Selected template: {0}"), FText::FromString(SelectedTemplate->Name))
					: FText::GetEmpty();
			})
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text_Lambda([this]() { return SelectedTemplate.IsValid() ? FText::FromString(SelectedTemplate->Description) : FText::GetEmpty(); })
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		]

		// What happens to the current tree
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 10.f, 0.f, 10.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
			.Padding(FMargin(10.f, 8.f))
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Top)
				.Padding(0.f, 1.f, 8.f, 0.f)
				[
					SNew(SImage)
					.Image_Lambda([HasPillars]() { return FAppStyle::Get().GetBrush(HasPillars() ? "Icons.WarningWithColor" : "Icons.InfoWithColor"); })
					.DesiredSizeOverride(FVector2D(16.f, 16.f))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(STextBlock)
					.Text_Lambda([HasPillars]()
					{
						if (!HasPillars())
						{
							return LOCTEXT("NothingToOverwrite", "Your pillar tree is empty, so nothing will be overwritten.");
						}
						const FProtoBoyPillarService& Service = UProtoBoySubsystem::Get()->GetPillars();
						return FText::Format(LOCTEXT("WillOverwrite", "This overwrites your current pillar tree: all {0} pillars below {1} are replaced by the template's. Docs already generated for them are kept, and marked archived the next time you generate docs."),
							FText::AsNumber(Service.GetPillars().Num() - 1), FText::FromString(Service.GetRoot().Name));
					})
					.AutoWrapText(true)
				]
			]
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SAssignNew(ConfirmPreview, SProtoBoyPillarTreePreview)
		];

	const TSharedRef<SWidget> Apply = SNew(SButton)
		.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
		.Text_Lambda([HasPillars]() { return HasPillars() ? LOCTEXT("OverwritePillars", "Overwrite Pillars") : LOCTEXT("ApplyTemplate", "Use Template"); })
		.IsEnabled_Lambda([this]() { return SelectedTemplate.IsValid() && UProtoBoySubsystem::Get() != nullptr; })
		.OnClicked(this, &SProtoBoySetupPillarsDialog::OnApplyTemplateClicked);

	return MakeSubStep(LOCTEXT("ConfirmTitle", "Confirm Template"), Body, EStep::FromTemplate, Apply);
}

FReply SProtoBoySetupPillarsDialog::OnApplyTemplateClicked()
{
	UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	if (!Subsystem || !SelectedTemplate.IsValid())
	{
		return FReply::Handled();
	}
	Subsystem->GetPillars().ReplacePillars(SelectedTemplate->Pillars);

	if (const TSharedPtr<SWindow> Window = ParentWindow.Pin())
	{
		Window->RequestDestroyWindow();
	}
	return FReply::Handled();
}

TSharedRef<SWidget> SProtoBoySetupPillarsDialog::MakeFromExistingProjectStep()
{
	// TODO: ProtoBoy holds no LLM API keys, so this goes through the connected agent (MCP): a prompt that has the agent
	// read the project (modules, content folders, docs, task board) and propose a pillar tree, which comes back here as
	// a preview for the user to accept or edit before it's applied.
	return MakeSubStep(LOCTEXT("FromExistingTitle", "Build From Existing Project"), SNullWidget::NullWidget, EStep::Choose);
}

TSharedRef<SWidget> SProtoBoySetupPillarsDialog::MakeSubStep(const FText& Title, const TSharedRef<SWidget>& Body, EStep BackTo, const TSharedRef<SWidget>& Action)
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

		// Back on the left, the step's action (if any) on the right
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("Back", "Back"))
				.OnClicked_Lambda([this, BackTo]()
				{
					ShowStep(BackTo);
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

void SProtoBoySetupPillarsDialog::Open(const TSharedPtr<SWidget>& ParentWidget)
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Setup Pillars from Template"))
		.ClientSize(FVector2D(1000.f, 620.f))
		.SupportsMinimize(false)
		.SupportsMaximize(false);

	Window->SetContent(SNew(SProtoBoySetupPillarsDialog).ParentWindow(Window));
	FSlateApplication::Get().AddModalWindow(Window, ParentWidget);
}

#undef LOCTEXT_NAMESPACE
