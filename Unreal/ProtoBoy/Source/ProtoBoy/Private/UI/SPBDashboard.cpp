#include "UI/SPBDashboard.h"

#include "Styling/CoreStyle.h"
#include "UI/SPBConnectDocsDialog.h"
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
			.HAlign(HAlign_Left)
			.Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("ConnectDocs", "Connect to Documentation..."))
				.OnClicked(this, &SProtoBoyDashboard::OnConnectDocsClicked)
			]
		]
	];
}

FReply SProtoBoyDashboard::OnConnectDocsClicked()
{
	SProtoBoyConnectDocsDialog::Open(AsShared());
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
