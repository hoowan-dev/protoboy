#include "UI/Tasks/SPBTasksPage.h"

#include "Docs/PBDocMap.h"
#include "HAL/PlatformProcess.h"
#include "PBSubsystem.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "Tasks/PBTaskProvider.h"
#include "Tasks/Trello/PBTrelloClient.h"
#include "Tasks/PBTaskBoard.h"
#include "UI/Docs/Map/SPBDocMapNode.h"
#include "UI/PBWidgets.h"
#include "UI/Tasks/SPBGenerateTasksDialog.h"
#include "UI/Tasks/Trello/SPBTrelloSyncDialog.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ProtoBoyTasksPage"

void SProtoBoyTasksPage::Construct(const FArguments& InArgs)
{
	IsUnlocked = InArgs._IsUnlocked;

	ChildSlot
	[
		SNew(SVerticalBox)

			// Sync with the connected task service, and whether the board matches the task categories
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 12.f)
			[
				SNew(SHorizontalBox)
				.Visibility_Lambda([this]() { return IsUnlocked.Get() ? EVisibility::Visible : EVisibility::Collapsed; })

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.Text_Lambda([this]()
					{
						return FText::Format(LOCTEXT("SyncTasks", "Sync with {0}"), GetBoard() ? GetBoard()->GetServiceName() : FText::GetEmpty());
					})
					.ToolTipText(LOCTEXT("SyncTasksTip", "Check that the board has a \"To Do - Uncategorized\" list and a \"To Do - <Category>\" list for each task category, and offer to fix it if not."))
					.IsEnabled_Lambda([this]() { return GetSyncState() != FProtoBoyTaskBoard::ESyncState::Checking; })
					.OnClicked_Lambda([this]() { OnSyncClicked(); return FReply::Handled(); })
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(8.f, 0.f, 0.f, 0.f)
				[
					SNew(SCircularThrobber)
					.Radius(8.f)
					.Visibility_Lambda([this]() { return GetSyncState() == FProtoBoyTaskBoard::ESyncState::Checking ? EVisibility::Visible : EVisibility::Collapsed; })
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(8.f, 0.f, 0.f, 0.f)
				[
					SNew(SImage)
					.Visibility_Lambda([this]()
					{
						return GetSyncState() == FProtoBoyTaskBoard::ESyncState::InSync || GetSyncState() == FProtoBoyTaskBoard::ESyncState::OutOfSync ? EVisibility::Visible : EVisibility::Collapsed;
					})
					.Image_Lambda([this]()
					{
						return FAppStyle::Get().GetBrush(GetSyncState() == FProtoBoyTaskBoard::ESyncState::InSync ? "Icons.SuccessWithColor" : "Icons.XCircle");
					})
					.ToolTipText_Lambda([this]()
					{
						return GetSyncState() == FProtoBoyTaskBoard::ESyncState::InSync
							? LOCTEXT("TasksInSync", "The board's To Do lists and labels match the task categories.")
							: LOCTEXT("TasksOutOfSync", "Out of sync: the board's To Do lists or labels don't match the task categories. Press Sync to see what's different and resync.");
					})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(10.f, 0.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return GetLastTaskSyncText(); })
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]

				// The box always takes the free space, so Generate Tasks stays on the right.
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				.Padding(8.f, 0.f, 0.f, 0.f)
				[
					SNew(SBox)
					[
						SNew(STextBlock)
						.Visibility_Lambda([this]() { return GetSyncState() == FProtoBoyTaskBoard::ESyncState::Failed ? EVisibility::Visible : EVisibility::Collapsed; })
						.Text_Lambda([this]() { return GetBoard() ? GetBoard()->GetSyncError() : FText::GetEmpty(); })
						.ColorAndOpacity(FStyleColors::Error)
						.AutoWrapText(true)
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(8.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("GenerateTasks", "Generate Tasks"))
					.ToolTipText(LOCTEXT("GenerateTasksTip", "Draft tasks for the task categories from their docs with an LLM."))
					.OnClicked_Lambda([this]() { SProtoBoyGenerateTasksDialog::Open(AsShared()); return FReply::Handled(); })
				]
			]

			// Why it's locked, and where to fix it (from the dashboard)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.Visibility_Lambda([this]() { return IsUnlocked.Get() ? EVisibility::Collapsed : EVisibility::Visible; })
				[
					InArgs._LockMessage.Widget
				]
			]

			// To Do: each task category's list (filled by RebuildTasksSection)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 12.f)
			[
				SNew(SBox)
				.Visibility_Lambda([this]() { return IsUnlocked.Get() ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					ProtoBoyWidgets::MakeSection(LOCTEXT("TasksToDoHeader", "To Do"), SAssignNew(TasksList, SBox))
				]
			]

			// In Progress: the board's mapped In Progress list (filled by RebuildTasksSection)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.Visibility_Lambda([this]() { return IsUnlocked.Get() ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					ProtoBoyWidgets::MakeSection(LOCTEXT("TasksInProgressHeader", "In Progress"), SAssignNew(InProgressList, SBox))
				]
			]

			// Done: the board's mapped Done list, most recent first (filled by RebuildTasksSection)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 12.f, 0.f, 0.f)
			[
				SNew(SBox)
				.Visibility_Lambda([this]() { return IsUnlocked.Get() ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					ProtoBoyWidgets::MakeSection(LOCTEXT("TasksDoneHeader", "Done"), SAssignNew(DoneList, SBox))
				]
			]
	];

	RebuildTasksSection();
	if (UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get())
	{
		// Marking a doc a task category (or renaming one) changes the rows; a sync changes the tasks.
		DocMapChangedHandle = Subsystem->GetDocMap().OnChanged.AddSP(this, &SProtoBoyTasksPage::RebuildTasksSection);
		BoardChangedHandle = Subsystem->GetTaskBoard().OnChanged.AddSP(this, &SProtoBoyTasksPage::RebuildTasksSection);
	}
}

SProtoBoyTasksPage::~SProtoBoyTasksPage()
{
	if (UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get())
	{
		Subsystem->GetDocMap().OnChanged.Remove(DocMapChangedHandle);
		Subsystem->GetTaskBoard().OnChanged.Remove(BoardChangedHandle);
	}
}

FProtoBoyTaskBoard* SProtoBoyTasksPage::GetBoard()
{
	UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	return Subsystem ? &Subsystem->GetTaskBoard() : nullptr;
}

FProtoBoyTaskBoard::ESyncState SProtoBoyTasksPage::GetSyncState()
{
	return GetBoard() ? GetBoard()->GetSyncState() : FProtoBoyTaskBoard::ESyncState::Unknown;
}

void SProtoBoyTasksPage::OnSyncClicked()
{
	if (FProtoBoyTaskBoard* Board = GetBoard())
	{
		// Out of sync: show what a resync would change.
		const TWeakPtr<SProtoBoyTasksPage> WeakThis = SharedThis(this);
		Board->CheckSync([WeakThis](const FProtoBoyTrelloCategorySync& Plan)
		{
			if (const TSharedPtr<SProtoBoyTasksPage> This = WeakThis.Pin())
			{
				SProtoBoyTrelloSyncDialog::Open(This, Plan);
			}
		});
	}
}

FText SProtoBoyTasksPage::GetLastTaskSyncText() const
{
	const TOptional<FDateTime> When = GetBoard() ? GetBoard()->GetLastSync() : TOptional<FDateTime>();
	if (!When.IsSet())
	{
		return LOCTEXT("NeverSynced", "Never synced");
	}
	// AsDateTime takes UTC and shows it in the user's time zone.
	return FText::Format(LOCTEXT("LastSynced", "Last synced {0}"), FText::AsDateTime(When.GetValue(), EDateTimeStyle::Medium, EDateTimeStyle::Short));
}

TSharedRef<SWidget> SProtoBoyTasksPage::MakeTaskItem(const FProtoBoyTask& Task, bool bShowCategory)
{
	// Category chips: the task's labels that name a task category on the doc map.
	TSharedRef<SHorizontalBox> Chips = SNew(SHorizontalBox);
	if (bShowCategory)
	{
		const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
		const TArray<const FProtoBoyDocNode*> Categories = Subsystem ? Subsystem->GetDocMap().GetTaskCategories() : TArray<const FProtoBoyDocNode*>();
		for (const FString& Label : Task.Labels)
		{
			const FProtoBoyDocNode* const* Doc = Categories.FindByPredicate([&Label](const FProtoBoyDocNode* D) { return D->Title.Equals(Label.TrimStartAndEnd(), ESearchCase::IgnoreCase); });
			if (!Doc)
			{
				continue;
			}
			Chips->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(6.f, 0.f, 0.f, 0.f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(SProtoBoyDocMapNode::GetLayerColor((*Doc)->Layer))
				.Padding(FMargin(6.f, 1.f))
				.ToolTipText(FText::Format(LOCTEXT("CategoryChipTip", "From the \"{0}\" task category."), FText::FromString((*Doc)->Title)))
				[
					SNew(STextBlock)
					.Text(FText::FromString((*Doc)->Title))
					.TextStyle(FAppStyle::Get(), "SmallText")
					.ColorAndOpacity(FLinearColor::White)
				]
			];
		}
	}

	const FString Url = Task.Url;
	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Top)
		.Padding(0.f, 0.f, 6.f, 0.f)
		[
			SNew(STextBlock).Text(FText::FromString(TEXT("•")))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Top)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Task.Title))
				.ToolTipText(FText::FromString(Task.Description.Left(400)))
				.AutoWrapText(true)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				Chips
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Top)
		.Padding(8.f, 0.f, 0.f, 0.f)
		[
			SNew(SHyperlink)
			.Visibility(Url.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
			.Text(LOCTEXT("OpenTask", "Open"))
			.ToolTipText(FText::FromString(Url))
			.OnNavigate_Lambda([Url]() { FPlatformProcess::LaunchURL(*Url, nullptr, nullptr); })
		];
}

TSharedRef<SWidget> SProtoBoyTasksPage::MakeTaskCategoryRow(const FString& DocId, const FString& Category, const FText& Detail, const FLinearColor& Color)
{
	const FProtoBoyTaskBoard* Board = GetBoard();
	const bool bLoaded = Board && Board->IsLoaded();
	const FProtoBoyTrelloCategorySync::FCategoryList* List = Board ? Board->FindList(DocId) : nullptr;
	// Opened rows are remembered by doc, so renaming a category keeps it open.
	const FString Key = DocId.IsEmpty() ? FString(TEXT("<uncategorized>")) : DocId;

	// Count on the right of the header: the open cards in the category's To Do list.
	FText Count;
	FText CountTip;
	if (!bLoaded)
	{
		Count = FText::FromString(TEXT("-"));
		CountTip = LOCTEXT("CountNotLoaded", "Press Sync to load the To Do tasks.");
	}
	else if (!List)
	{
		Count = LOCTEXT("CountNoList", "No list");
		CountTip = FText::Format(LOCTEXT("CountNoListTip", "The board has no \"{0}\" list yet. Press Sync to add it."),
			FText::FromString(FProtoBoyTrelloClient::GetCategoryListName(Category)));
	}
	else
	{
		Count = FText::Format(LOCTEXT("CountToDo", "{0} to do"), FText::AsNumber(List->Tasks.Num()));
		CountTip = FText::Format(LOCTEXT("CountToDoTip", "Open cards in \"{0}\" at the last sync."),
			FText::FromString(FProtoBoyTrelloClient::GetCategoryListName(Category)));
	}

	// The sublist, shown when the category is opened.
	TSharedRef<SVerticalBox> Tasks = SNew(SVerticalBox);
	auto AddNote = [&Tasks](const FText& Text)
	{
		Tasks->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(Text)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		];
	};
	if (!bLoaded || !List)
	{
		AddNote(CountTip);
	}
	else if (List->Tasks.IsEmpty())
	{
		AddNote(LOCTEXT("NoToDoTasks", "No To Do tasks in this category."));
	}
	else
	{
		for (const FProtoBoyTask& Task : List->Tasks)
		{
			Tasks->AddSlot()
			.AutoHeight()
			.Padding(0.f, 2.f)
			[
				MakeTaskItem(Task)
			];
		}
	}

	return SNew(SExpandableArea)
		.InitiallyCollapsed(!ExpandedTaskCategories.Contains(Key))
		.OnAreaExpansionChanged_Lambda([this, Key](bool bExpanded)
		{
			if (bExpanded)
			{
				ExpandedTaskCategories.Add(Key);
			}
			else
			{
				ExpandedTaskCategories.Remove(Key);
			}
		})
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		.BodyBorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
		.HeaderPadding(FMargin(4.f, 2.f, 10.f, 2.f))
		.Padding(FMargin(30.f, 6.f, 10.f, 8.f))
		.HeaderContent()
		[
			SNew(SHorizontalBox)

			// Layer color, as on the doc map
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.f, 0.f, 0.f, 0.f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(Color)
				.Padding(FMargin(2.f, 0.f))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(10.f, 4.f, 8.f, 4.f)
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("Icons.Clipboard"))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			.Padding(0.f, 4.f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(Category))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(Detail)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(Count)
				.ToolTipText(CountTip)
				.ColorAndOpacity(List && !List->Tasks.IsEmpty() ? FSlateColor::UseForeground() : FSlateColor::UseSubduedForeground())
			]
		]
		.BodyContent()
		[
			Tasks
		];
}

void SProtoBoyTasksPage::RebuildTasksSection()
{
	if (!TasksList.IsValid())
	{
		return;
	}
	const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	const FProtoBoyDocMap* Map = Subsystem ? &Subsystem->GetDocMap() : nullptr;

	// In doc map order, so categories read like the map does.
	const TArray<const FProtoBoyDocNode*> Categories = Map ? Map->GetTaskCategories() : TArray<const FProtoBoyDocNode*>();

	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	if (Categories.IsEmpty())
	{
		Rows->AddSlot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 8.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoTaskCategories", "No task categories yet. On the doc map, right-click a doc and pick Task Category, or tick Task category under the selected doc."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		];
	}
	for (const FProtoBoyDocNode* Doc : Categories)
	{
		Rows->AddSlot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 4.f)
		[
			MakeTaskCategoryRow(Doc->Id, Doc->Title,
				FText::Format(LOCTEXT("TaskCategoryDetail", "{0} doc · {1}"), ProtoBoyDocLayers::GetDisplayName(Doc->Layer), FText::FromString(Doc->Path)),
				SProtoBoyDocMapNode::GetLayerColor(Doc->Layer))
		];
	}

	// Tasks that belong to no doc, last.
	Rows->AddSlot()
	.AutoHeight()
	.Padding(0.f, 0.f, 0.f, 4.f)
	[
		MakeTaskCategoryRow(FString(), ProtoBoyTasks::UncategorizedName,
			LOCTEXT("UncategorizedDetail", "Tasks that belong to no doc"),
			FLinearColor(0.25f, 0.25f, 0.25f))
	];

	TasksList->SetContent(Rows);

	static const TArray<FProtoBoyTask> NoTasks;
	const FProtoBoyTaskBoard* Board = GetBoard();
	FillTaskSection(InProgressList, Board ? Board->GetInProgress() : NoTasks,
		LOCTEXT("InProgressNotLoaded", "Press Sync to load the tasks in progress."), LOCTEXT("NoInProgress", "Nothing in progress."));
	FillTaskSection(DoneList, Board ? Board->GetDone() : NoTasks,
		LOCTEXT("DoneNotLoaded", "Press Sync to load the finished tasks."), LOCTEXT("NoDone", "Nothing done yet."), /*MaxShown*/ 20);
}

void SProtoBoyTasksPage::FillTaskSection(const TSharedPtr<SBox>& Box, const TArray<FProtoBoyTask>& Tasks, const FText& NotLoaded, const FText& Empty, int32 MaxShown)
{
	if (!Box.IsValid())
	{
		return;
	}
	TSharedRef<SVerticalBox> Items = SNew(SVerticalBox);
	auto AddNote = [&Items](const FText& Text)
	{
		Items->AddSlot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(STextBlock)
			.Text(Text)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		];
	};
	if (!GetBoard() || !GetBoard()->IsLoaded())
	{
		AddNote(NotLoaded);
	}
	else if (Tasks.IsEmpty())
	{
		AddNote(Empty);
	}
	else
	{
		const int32 NumShown = MaxShown > 0 ? FMath::Min(MaxShown, Tasks.Num()) : Tasks.Num();
		for (int32 Index = 0; Index < NumShown; ++Index)
		{
			Items->AddSlot()
			.AutoHeight()
			.Padding(0.f, 2.f)
			[
				MakeTaskItem(Tasks[Index], /*bShowCategory*/ true)
			];
		}
		if (NumShown < Tasks.Num())
		{
			AddNote(FText::Format(LOCTEXT("MoreTasks", "...and {0} more on the board."), FText::AsNumber(Tasks.Num() - NumShown)));
		}
	}
	Box->SetContent(Items);
}

#undef LOCTEXT_NAMESPACE
