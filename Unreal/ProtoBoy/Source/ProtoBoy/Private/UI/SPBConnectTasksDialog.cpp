#include "UI/SPBConnectTasksDialog.h"

#include "Framework/Application/SlateApplication.h"
#include "PBSubsystem.h"
#include "UI/Tasks/Trello/SPBTrelloPanel.h"
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
	// Open on the connected provider's page, if any.
	if (const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get())
	{
		SelectedProvider = Subsystem->GetTrelloConnector().GetConnection().Provider;
	}

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

				+ SWidgetSwitcher::Slot() [ MakeEmptyPanel() ]          // EProtoBoyTaskProvider::None
				+ SWidgetSwitcher::Slot() [ SNew(SProtoBoyTrelloPanel) ] // EProtoBoyTaskProvider::Trello
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

void SProtoBoyConnectTasksDialog::SelectProvider(EProvider Provider)
{
	SelectedProvider = Provider;
	PanelSwitcher->SetActiveWidgetIndex(static_cast<int32>(Provider));
}

#undef LOCTEXT_NAMESPACE
