#include "UI/Architecture/SPBSetupPillarsDialog.h"

#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "SProtoBoySetupPillarsDialog"

void SProtoBoySetupPillarsDialog::Construct(const FArguments& InArgs)
{
	ParentWindow = InArgs._ParentWindow;

	// TODO: Setup Pillars content.
	ChildSlot
	[
		SNullWidget::NullWidget
	];
}

void SProtoBoySetupPillarsDialog::Open(const TSharedPtr<SWidget>& ParentWidget)
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Setup Pillars"))
		.ClientSize(FVector2D(720.f, 480.f))
		.SupportsMinimize(false)
		.SupportsMaximize(false);

	Window->SetContent(SNew(SProtoBoySetupPillarsDialog).ParentWindow(Window));
	FSlateApplication::Get().AddModalWindow(Window, ParentWidget);
}

#undef LOCTEXT_NAMESPACE
