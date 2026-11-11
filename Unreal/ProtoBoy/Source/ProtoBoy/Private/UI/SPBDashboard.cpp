#include "UI/SPBDashboard.h"

#include "PBSubsystem.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "UI/PBStyle.h"
#include "UI/SPBConnectDocsDialog.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SProtoBoyDashboard"

void SProtoBoyDashboard::Construct(const FArguments& InArgs)
{
	// TODO: Status header, Run Health Check, New ADR, issue list.
	ChildSlot
	[
		SNew(SBox)
		.Padding(16.f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Title", "ProtoBoy"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 32))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Tagline", "Build games, not slop"))
				.Font(FCoreStyle::GetDefaultFontStyle("Italic", 14))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]

			// Documentation section
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 24.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("DocsHeader", "Documentation"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 8.f, 0.f, 0.f)
			[
				SAssignNew(DocsSection, SBox)
			]
		]
	];

	RebuildDocsSection();

	if (UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get())
	{
		ConnectionChangedHandle = Subsystem->GetObsidianConnector().OnConnectionChanged.AddSP(
			this, &SProtoBoyDashboard::RebuildDocsSection);
	}
}

SProtoBoyDashboard::~SProtoBoyDashboard()
{
	if (UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get())
	{
		Subsystem->GetObsidianConnector().OnConnectionChanged.Remove(ConnectionChangedHandle);
	}
}

void SProtoBoyDashboard::RebuildDocsSection()
{
	const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	const FProtoBoyObsidianConnector* Obsidian = Subsystem ? &Subsystem->GetObsidianConnector() : nullptr;

	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);

	if (Obsidian && Obsidian->IsConnected())
	{
		Content->AddSlot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(SImage)
				.Image(FProtoBoyStyle::Get().GetBrush("ProtoBoy.Icon.Obsidian"))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("ObsidianConnected", "Obsidian connected (vault \"{0}\")"),
					FText::FromString(Obsidian->GetVaultName())))
				.ColorAndOpacity(FStyleColors::Success)
			]
		];

		Content->AddSlot()
		.AutoHeight()
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Obsidian->GetDocsRoot()))
			.ToolTipText(LOCTEXT("DocsRootTip", "Folder ProtoBoy writes docs into."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		];
	}
	else
	{
		Content->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NotConnected", "No documentation platform connected."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
	}

	const bool bConnected = Obsidian && Obsidian->IsConnected();
	Content->AddSlot()
	.AutoHeight()
	.HAlign(HAlign_Left)
	.Padding(0.f, 8.f, 0.f, 0.f)
	[
		SNew(SButton)
		.Text(bConnected
			? LOCTEXT("ManageDocs", "Documentation Settings...")
			: LOCTEXT("ConnectDocs", "Connect to Documentation..."))
		.OnClicked(this, &SProtoBoyDashboard::OnConnectDocsClicked)
	];

	DocsSection->SetContent(Content);
}

FReply SProtoBoyDashboard::OnConnectDocsClicked()
{
	SProtoBoyConnectDocsDialog::Open(AsShared());
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
