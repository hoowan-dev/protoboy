#include "UI/SPBConnectTasksDialog.h"

#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "UI/PBStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SProtoBoyConnectTasksDialog"

void SProtoBoyConnectTasksDialog::Construct(const FArguments& InArgs)
{
	// TODO: Open on the connected provider's page once the connection is persisted.
	ChildSlot
	[
		SNew(SHorizontalBox)

		// Sidebar: Board Providers
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
						.Text(LOCTEXT("ProvidersHeader", "Board Providers"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						MakeProviderEntry(EProvider::Trello, LOCTEXT("Trello", "Trello"), "ProtoBoy.Icon.Trello")
					]
				]
			]
		]

		// Main panel: setup for the selected provider
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
			.Padding(16.f)
			[
				SAssignNew(PanelSwitcher, SWidgetSwitcher)
				.WidgetIndex(static_cast<int32>(SelectedProvider))

				+ SWidgetSwitcher::Slot() [ MakeEmptyPanel() ]                                                         // None
				+ SWidgetSwitcher::Slot() [ MakeProviderPanel(LOCTEXT("Trello", "Trello"), "ProtoBoy.Icon.Trello.Large") ] // Trello
			]
		]
	];
}

void SProtoBoyConnectTasksDialog::Open(const TSharedPtr<SWidget>& ParentWidget)
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Connect Task Board"))
		.ClientSize(FVector2D(720.f, 440.f))
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		[
			SNew(SProtoBoyConnectTasksDialog)
		];

	FSlateApplication::Get().AddModalWindow(Window, ParentWidget);
}

TSharedRef<SWidget> SProtoBoyConnectTasksDialog::MakeProviderEntry(EProvider Provider, const FText& Label, const FName& IconName)
{
	// Toggle-style checkbox so the selected provider stays highlighted, like a sidebar tab.
	return SNew(SCheckBox)
		.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
		.IsChecked_Lambda([this, Provider]()
		{
			return SelectedProvider == Provider ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.OnCheckStateChanged_Lambda([this, Provider](ECheckBoxState)
		{
			SelectProvider(Provider);
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

TSharedRef<SWidget> SProtoBoyConnectTasksDialog::MakeEmptyPanel()
{
	return SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SelectProvider", "Select a board provider."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
}

TSharedRef<SWidget> SProtoBoyConnectTasksDialog::MakeProviderPanel(const FText& ProviderName, const FName& LargeIconName)
{
	// Intentionally blank for now: just the provider header.
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 10.f, 0.f)
			[
				SNew(SImage)
				.Image(FProtoBoyStyle::Get().GetBrush(LargeIconName))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(ProviderName)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
			]
		];
}

void SProtoBoyConnectTasksDialog::SelectProvider(EProvider Provider)
{
	SelectedProvider = Provider;
	PanelSwitcher->SetActiveWidgetIndex(static_cast<int32>(Provider));
}

#undef LOCTEXT_NAMESPACE
