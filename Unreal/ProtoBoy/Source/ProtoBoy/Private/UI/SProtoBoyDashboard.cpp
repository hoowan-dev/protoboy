#include "UI/SProtoBoyDashboard.h"

#include "Widgets/Layout/SBox.h"
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
			SNew(STextBlock)
			.Text(LOCTEXT("Placeholder", "ProtoBoy dashboard: coming soon."))
		]
	];
}

#undef LOCTEXT_NAMESPACE
