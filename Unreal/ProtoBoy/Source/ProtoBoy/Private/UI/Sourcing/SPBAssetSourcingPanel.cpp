#include "UI/Sourcing/SPBAssetSourcingPanel.h"

#include "Docs/PBDocGenerator.h"
#include "PBSettings.h"
#include "PBSubsystem.h"
#include "Sourcing/PBAssetSourcing.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "UI/PBStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SProtoBoyAssetSourcingPanel"

void SProtoBoyAssetSourcingPanel::Construct(const FArguments& InArgs)
{
	RefreshArtDirectionStatus();
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
						.Text(LOCTEXT("FabDesc", "Agents can search Fab for assets that fit your design and art direction. Searches open in your browser and you pick the listings; ProtoBoy doesn't read Fab results (Fab has no public search API)."))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.AutoWrapText(true)
					]
				]
			]
		]

		// Art direction
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 10.f, 0.f, 4.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ArtDirectionLabel", "Art direction doc (optional)"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SEditableTextBox)
			.Text(FText::FromString(GetDefault<UProtoBoySettings>()->ArtDirectionDocPath))
			.HintText(LOCTEXT("ArtDirectionHint", "e.g. Art Direction.md (in the docs folder)"))
			.ToolTipText(LOCTEXT("ArtDirectionTip", "The doc describing the art style, references and asset goals. Agents read it before searching for assets. Leave empty to let agents search the docs."))
			.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type)
			{
				FString Path = Text.ToString().TrimStartAndEnd();
				if (!Path.IsEmpty())
				{
					FString Normalized;
					FText Error;
					if (!FProtoBoyDocGenerator::NormalizeDocPath(Path, Normalized, Error))
					{
						return; // Keep the previous value; the box still shows what was typed.
					}
					Path = Normalized;
				}
				GetMutableDefault<UProtoBoySettings>()->ArtDirectionDocPath = Path;
				OnAssetSourcingChanged();
			})
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text_Lambda([this]() { return ArtDirectionStatus; })
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		];
}

void SProtoBoyAssetSourcingPanel::RefreshArtDirectionStatus()
{
	const FString Path = ProtoBoyAssetSourcing::GetArtDirectionDocPath();
	const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	if (Path.IsEmpty())
	{
		ArtDirectionStatus = LOCTEXT("ArtDirectionNone", "Not set: agents will look for art direction in the docs, or ask.");
	}
	else if (Subsystem && Subsystem->GetDocsProvider().DocExists(Path))
	{
		ArtDirectionStatus = FText::Format(LOCTEXT("ArtDirectionFound", "Agents read {0} before choosing assets."), FText::FromString(Path));
	}
	else
	{
		ArtDirectionStatus = FText::Format(LOCTEXT("ArtDirectionMissing", "{0} doesn't exist in the docs folder yet."), FText::FromString(Path));
	}
}

void SProtoBoyAssetSourcingPanel::OnAssetSourcingChanged()
{
	GetMutableDefault<UProtoBoySettings>()->TryUpdateDefaultConfigFile();
	RefreshArtDirectionStatus();

	// The agent guide states the asset policy.
	if (UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get())
	{
		Subsystem->GetMCPServer().RefreshAgentGuides();
	}
}

#undef LOCTEXT_NAMESPACE
