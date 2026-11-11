#include "UI/SPBConnectLLMDialog.h"

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

#define LOCTEXT_NAMESPACE "SProtoBoyConnectLLMDialog"

void SProtoBoyConnectLLMDialog::Construct(const FArguments& InArgs)
{
	// TODO: Open on the connected provider's page once the connection is persisted.
	ChildSlot
	[
		SNew(SHorizontalBox)

		// Sidebar: LLM Providers
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
						.Text(LOCTEXT("ProvidersHeader", "LLM Providers"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						MakeProviderEntry(EProtoBoyLLMProvider::Claude, LOCTEXT("Claude", "Claude"), "ProtoBoy.Icon.Claude")
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 2.f, 0.f, 0.f)
					[
						MakeProviderEntry(EProtoBoyLLMProvider::Gemini, LOCTEXT("Gemini", "Gemini"), "ProtoBoy.Icon.Gemini")
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

				+ SWidgetSwitcher::Slot() [ MakeEmptyPanel() ]                                    // None
				+ SWidgetSwitcher::Slot() [ MakeProviderPanel(LOCTEXT("Claude", "Claude")) ]      // Claude
				+ SWidgetSwitcher::Slot() [ MakeProviderPanel(LOCTEXT("Gemini", "Gemini")) ]      // Gemini
			]
		]
	];
}

void SProtoBoyConnectLLMDialog::Open(const TSharedPtr<SWidget>& ParentWidget)
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Connect LLM Provider"))
		.ClientSize(FVector2D(720.f, 440.f))
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		[
			SNew(SProtoBoyConnectLLMDialog)
		];

	FSlateApplication::Get().AddModalWindow(Window, ParentWidget);
}

TSharedRef<SWidget> SProtoBoyConnectLLMDialog::MakeProviderEntry(
	EProtoBoyLLMProvider Provider, const FText& Label, const FName& IconName)
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

TSharedRef<SWidget> SProtoBoyConnectLLMDialog::MakeEmptyPanel()
{
	return SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SelectProvider", "Select an LLM provider."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
}

TSharedRef<SWidget> SProtoBoyConnectLLMDialog::MakeProviderPanel(const FText& ProviderName)
{
	// Intentionally blank for now: just the provider name as a header.
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(ProviderName)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
		];
}

void SProtoBoyConnectLLMDialog::SelectProvider(EProtoBoyLLMProvider Provider)
{
	SelectedProvider = Provider;
	PanelSwitcher->SetActiveWidgetIndex(static_cast<int32>(Provider));
}

#undef LOCTEXT_NAMESPACE
