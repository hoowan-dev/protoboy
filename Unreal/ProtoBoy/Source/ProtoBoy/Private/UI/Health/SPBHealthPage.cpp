#include "UI/Health/SPBHealthPage.h"

#include "Health/PBDocHealth.h"
#include "UI/PBWidgets.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SProtoBoyHealthPage"

void SProtoBoyHealthPage::Construct(const FArguments& InArgs)
{
	IsUnlocked = InArgs._IsUnlocked;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return IsUnlocked.Get() ? EVisibility::Collapsed : EVisibility::Visible; })
			[
				InArgs._LockMessage.Widget
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return IsUnlocked.Get() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				ProtoBoyWidgets::MakeSection(LOCTEXT("ChecksHeader", "Documentation Health Checks"), MakeChecks())
			]
		]
	];
}

TSharedRef<SWidget> SProtoBoyHealthPage::MakeChecks()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ChecksDesc", "Find files changed in revision control that may be related to existing docs, so you can see which docs may need updating."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text_Lambda([]() { FText Status; ProtoBoyDocHealth::IsRevisionControlConnected(Status); return Status; })
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.Padding(0.f, 8.f, 0.f, 0.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("RunChecks", "Run Health Checks"))
			.ToolTipText(LOCTEXT("RunChecksTip", "Not implemented yet: does nothing."))
			.OnClicked_Lambda([]() { ProtoBoyDocHealth::RunHealthChecks(); return FReply::Handled(); })
		];
}

#undef LOCTEXT_NAMESPACE
