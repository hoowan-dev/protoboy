#include "UI/Architecture/SPBSetupPillarsDialog.h"

#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

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
	// TODO: List preset templates, filtered by game genre (platformer, shooter, RPG, ...) and scope (jam, prototype,
	// vertical slice, ...). Show the selected template's pillar tree as a preview, then apply it through the pillar
	// service (only when the tree has no pillars yet, or after the user confirms replacing them).
	return MakeSubStep(LOCTEXT("FromTemplateTitle", "Build From Template"), SNullWidget::NullWidget);
}

TSharedRef<SWidget> SProtoBoySetupPillarsDialog::MakeFromExistingProjectStep()
{
	// TODO: ProtoBoy holds no LLM API keys, so this goes through the connected agent (MCP): a prompt that has the agent
	// read the project (modules, content folders, docs, task board) and propose a pillar tree, which comes back here as
	// a preview for the user to accept or edit before it's applied.
	return MakeSubStep(LOCTEXT("FromExistingTitle", "Build From Existing Project"), SNullWidget::NullWidget);
}

TSharedRef<SWidget> SProtoBoySetupPillarsDialog::MakeSubStep(const FText& Title, const TSharedRef<SWidget>& Body)
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

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		[
			SNew(SButton)
			.Text(LOCTEXT("Back", "Back"))
			.OnClicked_Lambda([this]()
			{
				ShowStep(EStep::Choose);
				return FReply::Handled();
			})
		];
}

void SProtoBoySetupPillarsDialog::Open(const TSharedPtr<SWidget>& ParentWidget)
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Setup Pillars from Template"))
		.ClientSize(FVector2D(720.f, 480.f))
		.SupportsMinimize(false)
		.SupportsMaximize(false);

	Window->SetContent(SNew(SProtoBoySetupPillarsDialog).ParentWindow(Window));
	FSlateApplication::Get().AddModalWindow(Window, ParentWidget);
}

#undef LOCTEXT_NAMESPACE
