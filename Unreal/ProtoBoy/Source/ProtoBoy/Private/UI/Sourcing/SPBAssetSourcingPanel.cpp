#include "UI/Sourcing/SPBAssetSourcingPanel.h"

#include "PBSettings.h"
#include "PBSubsystem.h"
#include "Sourcing/PBAssetSourcing.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "UI/PBStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SProtoBoyAssetSourcingPanel"

void SProtoBoyAssetSourcingPanel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		MakeContent()
	];
}

TSharedRef<SWidget> SProtoBoyAssetSourcingPanel::MakeContent()
{
	auto Checked = [](bool bValue) { return bValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; };

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("AssetSourcingIntro", "Choose where agents get art, audio and other assets, so they don't default to generating them with AI. Shared by the team."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		]

		// Policy
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 8.f, 0.f, 0.f)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([Checked]() { return Checked(GetDefault<UProtoBoySettings>()->bPreferSourcedAssets); })
			.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
			{
				GetMutableDefault<UProtoBoySettings>()->bPreferSourcedAssets = State == ECheckBoxState::Checked;
				OnAssetSourcingChanged();
			})
			.ToolTipText(LOCTEXT("PreferSourcedTip", "Agents are told to use the sources below, or marked placeholders, instead of AI-generated assets."))
			[
				SNew(STextBlock).Text(LOCTEXT("PreferSourced", "Prefer sourced assets over AI-generated ones"))
			]
		]

		// Sources
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 10.f, 0.f, 4.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SourcesLabel", "Sources"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
			.Padding(8.f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Top)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([Checked]() { return Checked(GetDefault<UProtoBoySettings>()->bEnableFabSource); })
					.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
					{
						GetMutableDefault<UProtoBoySettings>()->bEnableFabSource = State == ECheckBoxState::Checked;
						OnAssetSourcingChanged();
					})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Top)
				.Padding(6.f, 0.f, 6.f, 0.f)
				[
					SNew(SImage)
					.Image(FProtoBoyStyle::Get().GetBrush("ProtoBoy.Icon.Fab"))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock).Text(LOCTEXT("FabName", "Fab"))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 2.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("FabDesc", "Agents can search Fab for assets that fit your design and the style in your Content docs. Searches open in your browser and you pick the listings; ProtoBoy doesn't read Fab results (Fab has no public search API)."))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.AutoWrapText(true)
					]
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 10.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("StyleNote", "The style assets must have is written in Content docs on the doc map: agents read the Content doc for an asset (its Style, What to Look For and What to Avoid sections) before searching for it."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		];
}

void SProtoBoyAssetSourcingPanel::OnAssetSourcingChanged()
{
	GetMutableDefault<UProtoBoySettings>()->TryUpdateDefaultConfigFile();

	// The agent guide states the asset policy.
	if (UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get())
	{
		Subsystem->GetMCPServer().RefreshAgentGuides();
	}
}

#undef LOCTEXT_NAMESPACE
