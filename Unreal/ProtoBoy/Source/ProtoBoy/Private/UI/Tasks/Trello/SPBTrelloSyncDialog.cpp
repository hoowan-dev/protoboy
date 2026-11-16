#include "UI/Tasks/Trello/SPBTrelloSyncDialog.h"

#include "Framework/Application/SlateApplication.h"
#include "PBSubsystem.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Tasks/PBTaskProvider.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ProtoBoyTrelloSyncDialog"

TWeakPtr<SWindow> SProtoBoyTrelloSyncDialog::OpenWindow;

void SProtoBoyTrelloSyncDialog::Open(const TSharedPtr<SWidget>& ParentWidget, const FProtoBoyTrelloCategorySync& Plan, FOnFinished OnFinished)
{
	if (const TSharedPtr<SWindow> Existing = OpenWindow.Pin())
	{
		Existing->RequestDestroyWindow();
	}

	const TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Trello Board Out of Sync"))
		.ClientSize(FVector2D(560.f, 360.f))
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		[
			SNew(SProtoBoyTrelloSyncDialog)
			.Plan(Plan)
			.OnFinished(MoveTemp(OnFinished))
		];
	OpenWindow = Window;

	const TSharedPtr<SWindow> Parent = ParentWidget.IsValid() ? FSlateApplication::Get().FindWidgetWindow(ParentWidget.ToSharedRef()) : nullptr;
	if (Parent.IsValid())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(Window, Parent.ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(Window);
	}
}

void SProtoBoyTrelloSyncDialog::Construct(const FArguments& InArgs)
{
	Plan = InArgs._Plan;
	OnFinished = InArgs._OnFinished;

	TSharedRef<SVerticalBox> Actions = SNew(SVerticalBox);
	for (const FText& Action : DescribeActions())
	{
		Actions->AddSlot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)[ SNew(STextBlock).Text(FText::FromString(TEXT("•"))) ]
			+ SHorizontalBox::Slot().FillWidth(1.f)[ SNew(STextBlock).Text(Action).AutoWrapText(true) ]
		];
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		.Padding(16.f)
		[
			SNew(SVerticalBox)

			// What's wrong
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 10.f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Top)
				.Padding(0.f, 0.f, 8.f, 0.f)
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.XCircle"))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(STextBlock)
					.Text(FText::Format(LOCTEXT("Explanation", "The Trello board's To Do lists don't match the doc map's task categories. The board needs one \"{0}\" list plus one \"To Do - <Category>\" list per task category. Resync will:"),
						FText::FromString(FProtoBoyTrelloClient::GetCategoryListName(ProtoBoyTasks::UncategorizedName))))
					.AutoWrapText(true)
				]
			]

			// What Resync does
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
				.Padding(10.f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()[ Actions ]
				]
			]

			// Progress or error
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				.Visibility_Lambda([this]() { return bSyncing || !ErrorText.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 8.f, 0.f)
				[
					SNew(SCircularThrobber)
					.Radius(8.f)
					.Visibility_Lambda([this]() { return bSyncing ? EVisibility::Visible : EVisibility::Collapsed; })
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return bSyncing ? LOCTEXT("Syncing", "Syncing with Trello...") : ErrorText; })
					.ColorAndOpacity_Lambda([this]() { return bSyncing ? FSlateColor::UseForeground() : FStyleColors::Error; })
					.AutoWrapText(true)
				]
			]

			// Buttons
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(0.f, 12.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 8.f, 0.f)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
					.Text(LOCTEXT("Resync", "Resync"))
					.IsEnabled_Lambda([this]() { return !bSyncing && ErrorText.IsEmpty(); })
					.OnClicked(this, &SProtoBoyTrelloSyncDialog::OnResyncClicked)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text_Lambda([this]() { return ErrorText.IsEmpty() ? LOCTEXT("Cancel", "Cancel") : LOCTEXT("Close", "Close"); })
					.IsEnabled_Lambda([this]() { return !bSyncing; })
					.OnClicked_Lambda([this]() { CloseWindow(); return FReply::Handled(); })
				]
			]
		]
	];
}

TArray<FText> SProtoBoyTrelloSyncDialog::DescribeActions() const
{
	const FText Uncategorized = FText::FromString(FProtoBoyTrelloClient::GetCategoryListName(ProtoBoyTasks::UncategorizedName));
	TArray<FText> Lines;
	for (const FString& Name : Plan.ListsToCreate)
	{
		Lines.Add(FText::Format(LOCTEXT("AddList", "Add the list \"{0}\"."), FText::FromString(Name)));
	}
	for (const FProtoBoyTrelloCategorySync::FExtraList& List : Plan.ListsToRemove)
	{
		Lines.Add(List.NumCards > 0
			? FText::Format(LOCTEXT("RemoveList", "Move {1} {1}|plural(one=card,other=cards) from \"{0}\" to \"{2}\", then delete \"{0}\" (no task category is called that). Trello archives deleted lists."),
				FText::FromString(List.Name), FText::AsNumber(List.NumCards), Uncategorized)
			: FText::Format(LOCTEXT("RemoveEmptyList", "Delete the empty list \"{0}\" (no task category is called that). Trello archives deleted lists."),
				FText::FromString(List.Name)));
	}
	if (Plan.bRemapsToDoList)
	{
		Lines.Add(FText::Format(LOCTEXT("RemapToDo", "Use \"{0}\" as ProtoBoy's To Do list, since the current one is deleted (saved to the project's ProtoBoy settings)."), Uncategorized));
	}
	return Lines;
}

FReply SProtoBoyTrelloSyncDialog::OnResyncClicked()
{
	const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	if (!Subsystem)
	{
		return FReply::Handled();
	}
	bSyncing = true;
	ErrorText = FText::GetEmpty();

	const TWeakPtr<SProtoBoyTrelloSyncDialog> WeakThis = SharedThis(this);
	FOnFinished Finished = OnFinished;
	Subsystem->GetTrelloClient().ApplyCategorySync(Plan, [WeakThis, Finished](const TProtoBoyTaskResult<bool>& Result)
	{
		if (const TSharedPtr<SProtoBoyTrelloSyncDialog> Dialog = WeakThis.Pin())
		{
			Dialog->bSyncing = false;
			if (Result.bOk)
			{
				Dialog->CloseWindow();
			}
			else
			{
				Dialog->ErrorText = FText::Format(LOCTEXT("SyncFailed", "Resync stopped: {0} Changes made before this are kept; close this window and press Sync with Trello again to see what's left."), Result.Error);
			}
		}
		if (Finished)
		{
			Finished(Result.bOk);
		}
	});
	return FReply::Handled();
}

void SProtoBoyTrelloSyncDialog::CloseWindow()
{
	if (const TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared()))
	{
		Window->RequestDestroyWindow();
	}
}

#undef LOCTEXT_NAMESPACE
