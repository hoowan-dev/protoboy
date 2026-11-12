#include "UI/Architecture/SPBPillarDocsPreview.h"

#include "Framework/Application/SlateApplication.h"
#include "PBSubsystem.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SProtoBoyPillarDocsPreview"

namespace ProtoBoyPillarDocsPreview
{
	FText ActionLabel(EProtoBoyPillarDocAction Action)
	{
		switch (Action)
		{
		case EProtoBoyPillarDocAction::Create: return LOCTEXT("Create", "Create");
		case EProtoBoyPillarDocAction::Update: return LOCTEXT("Update", "Update");
		case EProtoBoyPillarDocAction::Archive: return LOCTEXT("Archive", "Archive");
		case EProtoBoyPillarDocAction::UpToDate: break;
		}
		return LOCTEXT("UpToDate", "Up to date");
	}

	FSlateColor ActionColor(EProtoBoyPillarDocAction Action)
	{
		switch (Action)
		{
		case EProtoBoyPillarDocAction::Create: return FStyleColors::Success;
		case EProtoBoyPillarDocAction::Update: return FStyleColors::AccentBlue;
		case EProtoBoyPillarDocAction::Archive: return FStyleColors::Warning;
		case EProtoBoyPillarDocAction::UpToDate: break;
		}
		return FSlateColor::UseSubduedForeground();
	}
}

void SProtoBoyPillarDocsPreview::Construct(const FArguments& InArgs)
{
	ParentWindow = InArgs._ParentWindow;

	if (const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get())
	{
		Changes = FProtoBoyPillarDocSync::Plan(Subsystem->GetPillars(), Subsystem->GetDocGenerator());
		DocsLocation = Subsystem->GetDocsProvider().GetDisplayName();
	}

	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	for (const FProtoBoyPillarDocChange& Change : Changes)
	{
		List->AddSlot().AutoHeight().Padding(0.f, 2.f)[ MakeChangeRow(Change) ];
	}
	if (Changes.IsEmpty())
	{
		List->AddSlot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Empty", "The pillar tree is empty. Add pillars in the Architecture section first."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
	}

	const int32 Pending = FProtoBoyPillarDocSync::CountPending(Changes);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		.Padding(16.f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(Pending == 0
					? LOCTEXT("NothingToDo", "Pillar docs are up to date.")
					: FText::Format(LOCTEXT("Summary", "{0} {0}|plural(one=change,other=changes) to write in {1}:"), FText::AsNumber(Pending), DocsLocation))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Safety", "ProtoBoy only writes new docs and its own header blocks. Text you've written in pillar docs isn't changed."))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.AutoWrapText(true)
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			.Padding(0.f, 10.f, 0.f, 0.f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
				.Padding(8.f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()[ List ]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 10.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return ApplyError; })
					.ColorAndOpacity(FStyleColors::Error)
					.AutoWrapText(true)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Cancel", "Cancel"))
					.OnClicked_Lambda([this]()
					{
						if (const TSharedPtr<SWindow> Window = ParentWindow.Pin())
						{
							Window->RequestDestroyWindow();
						}
						return FReply::Handled();
					})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
					.IsEnabled(Pending > 0)
					.Text(LOCTEXT("Apply", "Apply"))
					.OnClicked(this, &SProtoBoyPillarDocsPreview::OnApplyClicked)
				]
			]
		]
	];
}

void SProtoBoyPillarDocsPreview::Open(const TSharedPtr<SWidget>& ParentWidget)
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Generate Pillar Docs"))
		.ClientSize(FVector2D(720.f, 480.f))
		.SupportsMinimize(false)
		.SupportsMaximize(false);

	Window->SetContent(SNew(SProtoBoyPillarDocsPreview).ParentWindow(Window));
	FSlateApplication::Get().AddModalWindow(Window, ParentWidget);
}

TSharedRef<SWidget> SProtoBoyPillarDocsPreview::MakeChangeRow(const FProtoBoyPillarDocChange& Change)
{
	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Top)
		[
			SNew(SBox)
			.WidthOverride(80.f)
			[
				SNew(STextBlock)
				.Text(ProtoBoyPillarDocsPreview::ActionLabel(Change.Action))
				.ColorAndOpacity(ProtoBoyPillarDocsPreview::ActionColor(Change.Action))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			]
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("RowTitle", "{0}  ({1})"), FText::FromString(Change.Label), FText::FromString(Change.DocPath)))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Visibility(Change.Detail.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
				.Text(FText::FromString(Change.Detail))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.AutoWrapText(true)
			]
		];
}

FReply SProtoBoyPillarDocsPreview::OnApplyClicked()
{
	UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	if (!Subsystem)
	{
		return FReply::Handled();
	}

	int32 Written = 0;
	if (!FProtoBoyPillarDocSync::Apply(Changes, Subsystem->GetPillars(), Subsystem->GetDocGenerator(), Written, ApplyError))
	{
		return FReply::Handled();
	}

	if (const TSharedPtr<SWindow> Window = ParentWindow.Pin())
	{
		Window->RequestDestroyWindow();
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
