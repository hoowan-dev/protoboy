#include "UI/MCP/SPBConnectLLMDialog.h"

#include "Framework/Application/SlateApplication.h"
#include "MCP/PBMCPClientConfig.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "UI/MCP/SPBMCPClientPanel.h"
#include "UI/MCP/SPBMCPServerPanel.h"
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
	auto ProviderEntry = [this](EPage Page, EProtoBoyLLMProvider Provider)
	{
		return MakePageEntry(Page, FProtoBoyMCPClientConfig::GetProviderName(Provider),
			FProtoBoyStyle::Get().GetBrush(FProtoBoyMCPClientConfig::GetProviderIcon(Provider)));
	};

	ChildSlot
	[
		SNew(SHorizontalBox)

		// Sidebar
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
					[
						MakeSidebarHeader(LOCTEXT("ServerHeader", "MCP"))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						MakePageEntry(EPage::Server, LOCTEXT("Server", "Server"), FAppStyle::Get().GetBrush("Icons.Settings"))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 12.f, 0.f, 0.f)
					[
						MakeSidebarHeader(LOCTEXT("ProvidersHeader", "LLM Providers"))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						ProviderEntry(EPage::Claude, EProtoBoyLLMProvider::Claude)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 2.f, 0.f, 0.f)
					[
						ProviderEntry(EPage::Gemini, EProtoBoyLLMProvider::Gemini)
					]
				]
			]
		]

		// Main panel: the selected page
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
			.Padding(16.f)
			[
				SAssignNew(PanelSwitcher, SWidgetSwitcher)
				.WidgetIndex(static_cast<int32>(SelectedPage))

				+ SWidgetSwitcher::Slot() [ SNew(SProtoBoyMCPServerPanel) ]                               // Server
				+ SWidgetSwitcher::Slot() [ SNew(SProtoBoyMCPClientPanel, EProtoBoyLLMProvider::Claude) ] // Claude
				+ SWidgetSwitcher::Slot() [ SNew(SProtoBoyMCPClientPanel, EProtoBoyLLMProvider::Gemini) ] // Gemini
			]
		]
	];
}

void SProtoBoyConnectLLMDialog::Open(const TSharedPtr<SWidget>& ParentWidget)
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Connect LLM Provider"))
		.ClientSize(FVector2D(800.f, 560.f))
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		[
			SNew(SProtoBoyConnectLLMDialog)
		];

	FSlateApplication::Get().AddModalWindow(Window, ParentWidget);
}

TSharedRef<SWidget> SProtoBoyConnectLLMDialog::MakeSidebarHeader(const FText& Text)
{
	return SNew(SBox)
		.Padding(4.f, 4.f, 4.f, 8.f)
		[
			SNew(STextBlock)
			.Text(Text)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		];
}

TSharedRef<SWidget> SProtoBoyConnectLLMDialog::MakePageEntry(EPage Page, const FText& Label, const FSlateBrush* Icon)
{
	// Toggle-style checkbox so the selected page stays highlighted, like a sidebar tab.
	return SNew(SCheckBox)
		.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
		.IsChecked_Lambda([this, Page]()
		{
			return SelectedPage == Page ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.OnCheckStateChanged_Lambda([this, Page](ECheckBoxState)
		{
			SelectPage(Page);
		})
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(6.f, 4.f)
			[
				SNew(SBox)
				.WidthOverride(20.f)
				.HeightOverride(20.f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(Icon)
				]
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

void SProtoBoyConnectLLMDialog::SelectPage(EPage Page)
{
	SelectedPage = Page;
	PanelSwitcher->SetActiveWidgetIndex(static_cast<int32>(Page));
}

#undef LOCTEXT_NAMESPACE
