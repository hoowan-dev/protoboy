#include "UI/SPBDashboard.h"

#include "Styling/CoreStyle.h"
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
		]
	];
}

#undef LOCTEXT_NAMESPACE
