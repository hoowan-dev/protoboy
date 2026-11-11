#include "UI/SPBConnectDocsDialog.h"

#include "Framework/Application/SlateApplication.h"
#include "PBSubsystem.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "UI/PBStyle.h"
#include "UI/SPBObsidianPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SProtoBoyConnectDocsDialog"

void SProtoBoyConnectDocsDialog::Construct(const FArguments& InArgs)
{
	// Open on the connected platform's page, if any.
	if (const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get())
	{
		SelectedPlatform = Subsystem->GetObsidianConnector().GetConnection().Platform;
	}

	ChildSlot
	[
		SNew(SHorizontalBox)

		// Sidebar: Documentation Platforms
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
			.Padding(8.f)
			[
				SNew(SBox)
				.WidthOverride(200.f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(4.f, 4.f, 4.f, 8.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("PlatformsHeader", "Documentation Platforms"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						MakePlatformEntry(EProtoBoyDocPlatform::Obsidian,
							LOCTEXT("Obsidian", "Obsidian"), "ProtoBoy.Icon.Obsidian")
					]
				]
			]
		]

		// Main panel: settings for the selected platform
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
			.Padding(16.f)
			[
				SAssignNew(PanelSwitcher, SWidgetSwitcher)
				.WidgetIndex(static_cast<int32>(SelectedPlatform))

				+ SWidgetSwitcher::Slot() [ MakeEmptyPanel() ]     // EProtoBoyDocPlatform::None
				+ SWidgetSwitcher::Slot() [ MakeObsidianPanel() ]  // EProtoBoyDocPlatform::Obsidian
			]
		]
	];
}

void SProtoBoyConnectDocsDialog::Open(const TSharedPtr<SWidget>& ParentWidget)
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Connect to Documentation"))
		.ClientSize(FVector2D(720.f, 440.f))
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		[
			SNew(SProtoBoyConnectDocsDialog)
		];

	FSlateApplication::Get().AddModalWindow(Window, ParentWidget);
}

TSharedRef<SWidget> SProtoBoyConnectDocsDialog::MakePlatformEntry(
	EProtoBoyDocPlatform Platform, const FText& Label, const FName& IconName)
{
	// Toggle-style checkbox so the selected platform stays highlighted, like a sidebar tab.
	return SNew(SCheckBox)
		.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
		.IsChecked_Lambda([this, Platform]()
		{
			return SelectedPlatform == Platform ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.OnCheckStateChanged_Lambda([this, Platform](ECheckBoxState)
		{
			SelectPlatform(Platform);
		})
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(6.f, 4.f)
			[
				SNew(SImage)
				.Image(FProtoBoyStyle::Get().GetBrush(IconName))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			.Padding(2.f, 4.f, 6.f, 4.f)
			[
				SNew(STextBlock)
				.Text(Label)
			]
		];
}

TSharedRef<SWidget> SProtoBoyConnectDocsDialog::MakeEmptyPanel()
{
	return SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SelectPlatform", "Select a documentation platform."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
}

TSharedRef<SWidget> SProtoBoyConnectDocsDialog::MakeObsidianPanel()
{
	return SNew(SProtoBoyObsidianPanel);
}

void SProtoBoyConnectDocsDialog::SelectPlatform(EProtoBoyDocPlatform Platform)
{
	SelectedPlatform = Platform;
	PanelSwitcher->SetActiveWidgetIndex(static_cast<int32>(Platform));
}

#undef LOCTEXT_NAMESPACE
