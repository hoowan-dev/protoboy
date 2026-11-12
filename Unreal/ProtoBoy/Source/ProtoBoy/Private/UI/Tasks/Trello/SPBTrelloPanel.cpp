#include "UI/Tasks/Trello/SPBTrelloPanel.h"

#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#include "PBSettings.h"
#include "PBSubsystem.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "Tasks/Trello/PBTrelloClient.h"
#include "Tasks/Trello/PBTrelloConnector.h"
#include "UI/PBStyle.h"
#include "Widgets/Images/SImage.h"
#include "Misc/App.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SProtoBoyTrelloPanel"

namespace ProtoBoyTrelloPanel
{
	bool IsBusy()
	{
		const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
		return Subsystem && Subsystem->GetTrelloConnector().GetState() != FProtoBoyTrelloConnector::EState::Idle;
	}

	bool IsConnected()
	{
		const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
		return Subsystem && Subsystem->GetTrelloConnector().IsConnected();
	}
}

void SProtoBoyTrelloPanel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SScrollBox)

		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MakeHeader()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Intro", "Connect opens Trello in your browser. Click Allow there and you'll be connected here automatically."))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.AutoWrapText(true)
			]

			// Status
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 16.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(this, &SProtoBoyTrelloPanel::GetStatusText)
				.ColorAndOpacity(this, &SProtoBoyTrelloPanel::GetStatusColor)
				.AutoWrapText(true)
			]

			// Actions
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			.Padding(0.f, 10.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
					.Visibility_Lambda([]() { return !ProtoBoyTrelloPanel::IsBusy() && !ProtoBoyTrelloPanel::IsConnected() ? EVisibility::Visible : EVisibility::Collapsed; })
					.Text(LOCTEXT("Connect", "Connect"))
					.OnClicked(this, &SProtoBoyTrelloPanel::OnConnectClicked)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Visibility_Lambda([]() { return ProtoBoyTrelloPanel::IsBusy() ? EVisibility::Visible : EVisibility::Collapsed; })
					.Text(LOCTEXT("Cancel", "Cancel"))
					.OnClicked(this, &SProtoBoyTrelloPanel::OnCancelClicked)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Visibility_Lambda([]() { return !ProtoBoyTrelloPanel::IsBusy() && ProtoBoyTrelloPanel::IsConnected() ? EVisibility::Visible : EVisibility::Collapsed; })
					.Text(LOCTEXT("Disconnect", "Disconnect"))
					.OnClicked(this, &SProtoBoyTrelloPanel::OnDisconnectClicked)
				]
			]

			// Board and list mapping (team-wide)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 20.f, 0.f, 0.f)
			[
				SNew(SBox)
				.Visibility_Lambda([]() { return ProtoBoyTrelloPanel::IsConnected() ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					MakeBoardSection()
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 20.f, 0.f, 0.f)
			[
				MakeSetupDetails()
			]
		]
	];

	if (UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get())
	{
		ConnectionChangedHandle = Subsystem->GetTrelloConnector().OnConnectionChanged.AddSP(this, &SProtoBoyTrelloPanel::OnConnectionChanged);
	}
	OnConnectionChanged();
}

SProtoBoyTrelloPanel::~SProtoBoyTrelloPanel()
{
	if (UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get())
	{
		Subsystem->GetTrelloConnector().OnConnectionChanged.Remove(ConnectionChangedHandle);
	}
}

void SProtoBoyTrelloPanel::OnConnectionChanged()
{
	if (!ProtoBoyTrelloPanel::IsConnected())
	{
		BoardOptions.Reset();
		ListOptions.Reset();
		BoardError = FText::GetEmpty();
		return;
	}
	if (BoardOptions.IsEmpty() && !bLoadingBoards)
	{
		RefreshBoards();
	}
	const FString& BoardId = GetDefault<UProtoBoySettings>()->TrelloBoardId;
	if (!BoardId.IsEmpty() && ListOptions.IsEmpty() && !bLoadingLists)
	{
		RefreshLists(BoardId);
	}
}

TSharedRef<SWidget> SProtoBoyTrelloPanel::MakeBoardSection()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
		.Padding(12.f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text_Lambda([]()
				{
					const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
					FText WhyNot;
					return Subsystem && Subsystem->GetTrelloClient().IsReady(WhyNot)
						? LOCTEXT("BoardHeader", "Project board")
						: LOCTEXT("BoardHeaderSetup", "Next: set up your project board");
				})
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("BoardIntro", "Shared by the whole team (saved to DefaultProtoBoy.ini). Agents can only see and change cards on this board."))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.AutoWrapText(true)
			]

			// Board
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 10.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(100.f)
					[
						SNew(STextBlock).Text(LOCTEXT("BoardLabel", "Board"))
					]
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SAssignNew(BoardCombo, SComboBox<FItemPtr>)
					.OptionsSource(&BoardOptions)
					.OnGenerateWidget_Lambda([](FItemPtr Item) -> TSharedRef<SWidget>
					{
						return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? Item->Name : FString()));
					})
					.OnSelectionChanged(this, &SProtoBoyTrelloPanel::OnBoardPicked)
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							if (bLoadingBoards)
							{
								return LOCTEXT("LoadingBoards", "Loading boards...");
							}
							const FString& Name = GetDefault<UProtoBoySettings>()->TrelloBoardName;
							return Name.IsEmpty() ? LOCTEXT("PickBoard", "Choose a board") : FText::FromString(Name);
						})
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(6.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Refresh", "Refresh"))
					.ToolTipText(LOCTEXT("RefreshTip", "Reload your boards and the chosen board's lists from Trello."))
					.OnClicked_Lambda([this]()
					{
						RefreshBoards();
						const FString& BoardId = GetDefault<UProtoBoySettings>()->TrelloBoardId;
						if (!BoardId.IsEmpty())
						{
							RefreshLists(BoardId);
						}
						return FReply::Handled();
					})
				]
			]

			// List mapping
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 10.f, 0.f, 0.f)
			[
				SNew(SVerticalBox)
				.Visibility_Lambda([]() { return GetDefault<UProtoBoySettings>()->TrelloBoardId.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })

				+ SVerticalBox::Slot().AutoHeight()[ MakeListRow(LOCTEXT("ToDo", "To Do"), &UProtoBoySettings::TrelloToDoListId) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)[ MakeListRow(LOCTEXT("InProgress", "In Progress"), &UProtoBoySettings::TrelloInProgressListId) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)[ MakeListRow(LOCTEXT("Done", "Done"), &UProtoBoySettings::TrelloDoneListId) ]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 12.f, 0.f, 0.f)
			[
				MakeTemplateActions()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 10.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(this, &SProtoBoyTrelloPanel::GetBoardStatusText)
				.ColorAndOpacity(this, &SProtoBoyTrelloPanel::GetBoardStatusColor)
				.AutoWrapText(true)
			]
		];
}

TSharedRef<SWidget> SProtoBoyTrelloPanel::MakeListRow(const FText& Label, FListField Field)
{
	TSharedPtr<SComboBox<FItemPtr>> Combo;

	TSharedRef<SWidget> Row = SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(100.f)
			[
				SNew(STextBlock).Text(Label)
			]
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		[
			SAssignNew(Combo, SComboBox<FItemPtr>)
			.OptionsSource(&ListOptions)
			.OnGenerateWidget_Lambda([](FItemPtr Item) -> TSharedRef<SWidget>
			{
				return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? Item->Name : FString()));
			})
			.OnSelectionChanged_Lambda([Field](FItemPtr Item, ESelectInfo::Type SelectInfo)
			{
				if (!Item.IsValid() || SelectInfo == ESelectInfo::Direct)
				{
					return;
				}
				UProtoBoySettings* Settings = GetMutableDefault<UProtoBoySettings>();
				Settings->*Field = Item->Id;
				Settings->TryUpdateDefaultConfigFile();
			})
			[
				SNew(STextBlock)
				.Text_Lambda([this, Field]()
				{
					if (bLoadingLists)
					{
						return LOCTEXT("LoadingLists", "Loading lists...");
					}
					return GetListName(GetDefault<UProtoBoySettings>()->*Field);
				})
			]
		];

	ListCombos.Add(Combo);
	return Row;
}

void SProtoBoyTrelloPanel::RefreshBoards()
{
	UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	if (!Subsystem)
	{
		return;
	}

	bLoadingBoards = true;
	BoardError = FText::GetEmpty();
	const TWeakPtr<SProtoBoyTrelloPanel> WeakThis = StaticCastSharedRef<SProtoBoyTrelloPanel>(AsShared());
	Subsystem->GetTrelloClient().FetchBoards([WeakThis](const TProtoBoyTaskResult<TArray<FProtoBoyTrelloItem>>& Result)
	{
		const TSharedPtr<SProtoBoyTrelloPanel> This = WeakThis.Pin();
		if (!This.IsValid())
		{
			return;
		}
		This->bLoadingBoards = false;
		This->BoardOptions.Reset();
		if (!Result.bOk)
		{
			This->BoardError = Result.Error;
		}
		for (const FProtoBoyTrelloItem& Board : Result.Value)
		{
			This->BoardOptions.Add(MakeShared<FProtoBoyTrelloItem>(Board));
		}
		if (This->BoardCombo.IsValid())
		{
			This->BoardCombo->RefreshOptions();
		}
	});
}

void SProtoBoyTrelloPanel::RefreshLists(const FString& BoardId)
{
	UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	if (!Subsystem)
	{
		return;
	}

	bLoadingLists = true;
	BoardError = FText::GetEmpty();
	const TWeakPtr<SProtoBoyTrelloPanel> WeakThis = StaticCastSharedRef<SProtoBoyTrelloPanel>(AsShared());
	Subsystem->GetTrelloClient().FetchLists(BoardId, [WeakThis](const TProtoBoyTaskResult<TArray<FProtoBoyTrelloItem>>& Result)
	{
		const TSharedPtr<SProtoBoyTrelloPanel> This = WeakThis.Pin();
		if (!This.IsValid())
		{
			return;
		}
		This->bLoadingLists = false;
		This->ListOptions.Reset();
		if (!Result.bOk)
		{
			This->BoardError = Result.Error;
		}
		for (const FProtoBoyTrelloItem& List : Result.Value)
		{
			This->ListOptions.Add(MakeShared<FProtoBoyTrelloItem>(List));
		}
		for (const TSharedPtr<SComboBox<FItemPtr>>& Combo : This->ListCombos)
		{
			if (Combo.IsValid())
			{
				Combo->RefreshOptions();
			}
		}
		This->AutoMapLists();
	});
}

void SProtoBoyTrelloPanel::OnBoardPicked(FItemPtr Board, ESelectInfo::Type SelectInfo)
{
	if (!Board.IsValid() || SelectInfo == ESelectInfo::Direct)
	{
		return;
	}

	UProtoBoySettings* Settings = GetMutableDefault<UProtoBoySettings>();
	if (Settings->TrelloBoardId == Board->Id)
	{
		return;
	}

	// A different board means different lists.
	Settings->TrelloBoardId = Board->Id;
	Settings->TrelloBoardName = Board->Name;
	Settings->TrelloToDoListId.Reset();
	Settings->TrelloInProgressListId.Reset();
	Settings->TrelloDoneListId.Reset();
	Settings->TryUpdateDefaultConfigFile();

	RefreshLists(Board->Id);
}

void SProtoBoyTrelloPanel::AutoMapLists()
{
	TArray<FProtoBoyTrelloItem> Lists;
	for (const FItemPtr& Item : ListOptions)
	{
		Lists.Add(*Item);
	}

	UProtoBoySettings* Settings = GetMutableDefault<UProtoBoySettings>();
	bool bChanged = false;
	auto Fill = [&](FString& Field, EProtoBoyTaskStatus Status)
	{
		const bool bKnown = Lists.ContainsByPredicate([&Field](const FProtoBoyTrelloItem& List) { return List.Id == Field; });
		if (Field.IsEmpty() || !bKnown)
		{
			const FString Guessed = FProtoBoyTrelloClient::GuessListId(Lists, Status);
			if (Guessed != Field)
			{
				Field = Guessed;
				bChanged = true;
			}
		}
	};

	Fill(Settings->TrelloToDoListId, EProtoBoyTaskStatus::ToDo);
	Fill(Settings->TrelloInProgressListId, EProtoBoyTaskStatus::InProgress);
	Fill(Settings->TrelloDoneListId, EProtoBoyTaskStatus::Done);

	if (bChanged)
	{
		Settings->TryUpdateDefaultConfigFile();
	}
}

TSharedRef<SWidget> SProtoBoyTrelloPanel::MakeTemplateActions()
{
	return SNew(SVerticalBox)

		// No board yet: create one from the template.
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SVerticalBox)
			.Visibility_Lambda([]() { return GetDefault<UProtoBoySettings>()->TrelloBoardId.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("TemplateIntro", "No board for this project yet? Create one from ProtoBoy's template: To Do, In Progress and Done lists, mapped automatically."))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.AutoWrapText(true)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 6.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SAssignNew(NewBoardNameBox, SEditableTextBox)
					.Text(FText::FromString(FString::Printf(TEXT("%s Tasks"), FApp::GetProjectName())))
					.HintText(LOCTEXT("BoardNameHint", "New board name"))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(6.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
					.Text(LOCTEXT("CreateBoard", "Create Board from Template"))
					.IsEnabled_Lambda([this]() { return !bApplyingTemplate; })
					.OnClicked_Lambda([this]() { ApplyTemplate(/*bNewBoard*/ true); return FReply::Handled(); })
				]
			]
		]

		// Board chosen but missing lists: add them.
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([this]() { return NeedsListSetup() ? EVisibility::Visible : EVisibility::Collapsed; })

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					return FText::Format(LOCTEXT("MissingLists", "This board doesn't have the lists ProtoBoy expects ({0}). Pick existing ones above, or add them."),
						FText::FromString(GetMissingListNames()));
				})
				.ColorAndOpacity(FStyleColors::Warning)
				.AutoWrapText(true)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("AddLists", "Add Missing Lists"))
				.ToolTipText(LOCTEXT("AddListsTip", "Creates only the missing lists on this Trello board and maps them. Existing lists and cards aren't touched."))
				.IsEnabled_Lambda([this]() { return !bApplyingTemplate; })
				.OnClicked_Lambda([this]() { ApplyTemplate(/*bNewBoard*/ false); return FReply::Handled(); })
			]
		];
}

bool SProtoBoyTrelloPanel::NeedsListSetup() const
{
	const UProtoBoySettings* Settings = GetDefault<UProtoBoySettings>();
	if (Settings->TrelloBoardId.IsEmpty() || bLoadingLists || !BoardError.IsEmpty())
	{
		return false;
	}
	return !GetMissingListNames().IsEmpty();
}

FString SProtoBoyTrelloPanel::GetMissingListNames() const
{
	const UProtoBoySettings* Settings = GetDefault<UProtoBoySettings>();
	auto IsMapped = [this](const FString& Id)
	{
		return !Id.IsEmpty() && ListOptions.ContainsByPredicate([&Id](const FItemPtr& Item) { return Item->Id == Id; });
	};

	TArray<FString> Missing;
	if (!IsMapped(Settings->TrelloToDoListId)) { Missing.Add(FProtoBoyTrelloClient::GetTemplateListName(EProtoBoyTaskStatus::ToDo)); }
	if (!IsMapped(Settings->TrelloInProgressListId)) { Missing.Add(FProtoBoyTrelloClient::GetTemplateListName(EProtoBoyTaskStatus::InProgress)); }
	if (!IsMapped(Settings->TrelloDoneListId)) { Missing.Add(FProtoBoyTrelloClient::GetTemplateListName(EProtoBoyTaskStatus::Done)); }
	return FString::Join(Missing, TEXT(", "));
}

void SProtoBoyTrelloPanel::ApplyTemplate(bool bNewBoard)
{
	UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	if (!Subsystem || bApplyingTemplate)
	{
		return;
	}

	const UProtoBoySettings* Settings = GetDefault<UProtoBoySettings>();
	const FString NewName = NewBoardNameBox.IsValid() ? NewBoardNameBox->GetText().ToString() : FString();

	bApplyingTemplate = true;
	BoardError = FText::GetEmpty();
	TemplateMessage = FText::GetEmpty();

	const TWeakPtr<SProtoBoyTrelloPanel> WeakThis = StaticCastSharedRef<SProtoBoyTrelloPanel>(AsShared());
	Subsystem->GetTrelloClient().ApplyBoardTemplate(
		bNewBoard ? FString() : Settings->TrelloBoardId,
		bNewBoard ? FString() : Settings->TrelloBoardName,
		NewName,
		[WeakThis](const TProtoBoyTaskResult<FProtoBoyTrelloBoardSetup>& Result)
		{
			// Save the mapping even if the panel was closed meanwhile: the board now exists on Trello.
			if (Result.bOk)
			{
				UProtoBoySettings* MutableSettings = GetMutableDefault<UProtoBoySettings>();
				MutableSettings->TrelloBoardId = Result.Value.BoardId;
				MutableSettings->TrelloBoardName = Result.Value.BoardName;
				MutableSettings->TrelloToDoListId = Result.Value.ToDoListId;
				MutableSettings->TrelloInProgressListId = Result.Value.InProgressListId;
				MutableSettings->TrelloDoneListId = Result.Value.DoneListId;
				MutableSettings->TryUpdateDefaultConfigFile();
			}

			const TSharedPtr<SProtoBoyTrelloPanel> This = WeakThis.Pin();
			if (!This.IsValid())
			{
				return;
			}
			This->bApplyingTemplate = false;
			if (!Result.bOk)
			{
				This->BoardError = Result.Error;
				return;
			}

			This->TemplateMessage = Result.Value.bCreatedBoard
				? FText::Format(LOCTEXT("CreatedBoard", "Created board \"{0}\"."), FText::FromString(Result.Value.BoardName))
				: Result.Value.CreatedLists.IsEmpty()
					? LOCTEXT("NothingMissing", "The board already had the lists ProtoBoy needs.")
					: FText::Format(LOCTEXT("AddedLists", "Added lists: {0}."), FText::FromString(FString::Join(Result.Value.CreatedLists, TEXT(", "))));

			This->RefreshBoards();
			This->RefreshLists(Result.Value.BoardId);
		});
}

FText SProtoBoyTrelloPanel::GetListName(const FString& ListId) const
{
	if (ListId.IsEmpty())
	{
		return LOCTEXT("PickList", "Choose a list");
	}
	for (const FItemPtr& Item : ListOptions)
	{
		if (Item->Id == ListId)
		{
			return FText::FromString(Item->Name);
		}
	}
	return LOCTEXT("UnknownList", "(list not found on this board)");
}

FText SProtoBoyTrelloPanel::GetBoardStatusText() const
{
	if (!BoardError.IsEmpty())
	{
		return BoardError;
	}
	if (bApplyingTemplate)
	{
		return LOCTEXT("ApplyingTemplate", "Setting up the board on Trello...");
	}
	const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	if (!Subsystem)
	{
		return FText::GetEmpty();
	}
	FText WhyNot;
	if (!Subsystem->GetTrelloClient().IsReady(WhyNot))
	{
		return WhyNot;
	}
	const FText Ready = LOCTEXT("BoardReady", "Ready: agents can read and update tasks on this board through ProtoBoy.");
	return TemplateMessage.IsEmpty() ? Ready : FText::Format(LOCTEXT("ReadyWithMessage", "{0} {1}"), TemplateMessage, Ready);
}

FSlateColor SProtoBoyTrelloPanel::GetBoardStatusColor() const
{
	if (!BoardError.IsEmpty())
	{
		return FStyleColors::Error;
	}
	const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	FText WhyNot;
	return Subsystem && Subsystem->GetTrelloClient().IsReady(WhyNot) ? FSlateColor(FStyleColors::Success) : FSlateColor(FStyleColors::Warning);
}

TSharedRef<SWidget> SProtoBoyTrelloPanel::MakeHeader()
{
	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.f, 0.f, 10.f, 0.f)
		[
			SNew(SImage)
			.Image(FProtoBoyStyle::Get().GetBrush("ProtoBoy.Icon.Trello.Large"))
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Header", "Trello"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
		];
}

TSharedRef<SWidget> SProtoBoyTrelloPanel::MakeStep(int32 Number, const FText& Title, const TSharedRef<SWidget>& Body)
{
	return SNew(SHorizontalBox)

		// Step number
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Top)
		.Padding(0.f, 0.f, 10.f, 0.f)
		[
			SNew(SBox)
			.WidthOverride(16.f)
			[
				SNew(STextBlock)
				.Text(FText::AsNumber(Number))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
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
				.Text(Title)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 3.f, 0.f, 0.f)
			[
				Body
			]
		];
}

TSharedRef<SWidget> SProtoBoyTrelloPanel::MakeSetupDetails()
{
	auto Paragraph = [](const FText& Text) -> TSharedRef<SWidget>
	{
		return SNew(STextBlock)
			.Text(Text)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true);
	};

	return SNew(SExpandableArea)
		// Open by default until a key is available, since Connect can't work without one.
		.InitiallyCollapsed(!FProtoBoyTrelloConnector::GetApiKey().IsEmpty())
		.AreaTitle(LOCTEXT("SetupTitle", "API key setup"))
		.BorderImage(FAppStyle::Get().GetBrush("NoBorder"))
		.BodyBorderImage(FAppStyle::Get().GetBrush("NoBorder"))
		.HeaderPadding(FMargin(0.f, 2.f))
		.Padding(FMargin(0.f, 6.f, 0.f, 0.f))
		.BodyContent()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
			.Padding(12.f)
			[
				SNew(SVerticalBox)

				// Which key is in use
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text_Lambda([]()
					{
						if (FProtoBoyTrelloConnector::GetApiKey().IsEmpty())
						{
							return LOCTEXT("KeyMissing", "No API key set yet. A key comes from a Trello Power-Up (Trello's name for an app registration). Create one once and the whole team can share its key:");
						}
						return GetDefault<UProtoBoySettings>()->TrelloApiKey.TrimStartAndEnd().IsEmpty()
							? LOCTEXT("KeyBuiltIn", "Using ProtoBoy's built-in Trello API key. To use your own instead:")
							: LOCTEXT("KeyProject", "Using the API key from Project Settings. To set up a different one:");
					})
					.ColorAndOpacity_Lambda([]()
					{
						return FProtoBoyTrelloConnector::GetApiKey().IsEmpty() ? FSlateColor(FStyleColors::Warning) : FSlateColor::UseForeground();
					})
					.AutoWrapText(true)
				]

				// 1. Create the Power-Up
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 12.f, 0.f, 0.f)
				[
					MakeStep(1, LOCTEXT("Step1Title", "Create a Power-Up"),
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							Paragraph(LOCTEXT("Step1Workspace", "You need a Trello Workspace you're an admin of (a personal one works)."))
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Left)
						.Padding(0.f, 4.f, 0.f, 0.f)
						[
							SNew(SHyperlink)
							.Text(LOCTEXT("Step1Link", "Open trello.com/power-ups/admin"))
							.ToolTipText(LOCTEXT("Step1LinkTip", "Opens Trello's Power-Up admin page in your browser."))
							.OnNavigate_Lambda([]() { FPlatformProcess::LaunchURL(TEXT("https://trello.com/power-ups/admin"), nullptr, nullptr); })
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.f, 4.f, 0.f, 0.f)
						[
							Paragraph(LOCTEXT("Step1Form", "Click New. Name it ProtoBoy, pick your Workspace, and fill in your contact details. The Iframe connector URL is only for Power-Ups that show UI inside Trello, which ProtoBoy doesn't: leave it blank if the form allows, otherwise enter any URL you control."))
						])
				]

				// 2. Generate the key
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 12.f, 0.f, 0.f)
				[
					MakeStep(2, LOCTEXT("Step2Title", "Generate the API key"),
						Paragraph(LOCTEXT("Step2Body", "Open the Power-Up's API key tab (sometimes called Trello Auth), not the OAuth 2.0 tab: ProtoBoy uses an API key and token, not OAuth 2.0. Click Generate a new API key and copy the key. Ignore the Secret on the same page: ProtoBoy never uses it, and it must not go into the project.")))
				]

				// 3. Allowed origin
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 12.f, 0.f, 0.f)
				[
					MakeStep(3, LOCTEXT("Step3Title", "Allow ProtoBoy's address"),
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							Paragraph(LOCTEXT("Step3Body", "On the same tab, add this under Allowed origins. Without it, Trello can't send your browser back to ProtoBoy after you click Allow."))
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.f, 6.f, 0.f, 0.f)
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(SBorder)
								.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
								.Padding(FMargin(8.f, 4.f))
								[
									SNew(STextBlock)
									.Text_Lambda([]() { return FText::FromString(FProtoBoyTrelloConnector::GetCallbackOrigin()); })
									.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
								]
							]

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(8.f, 0.f, 0.f, 0.f)
							[
								SNew(SButton)
								.Text(LOCTEXT("Copy", "Copy"))
								.OnClicked(this, &SProtoBoyTrelloPanel::OnCopyOriginClicked)
							]
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.f, 6.f, 0.f, 0.f)
						[
							Paragraph(LOCTEXT("Step3Port", "If you change Tasks Callback Port in Project Settings, update this origin to match."))
						])
				]

				// 4. Hand the key to ProtoBoy
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 12.f, 0.f, 0.f)
				[
					MakeStep(4, LOCTEXT("Step4Title", "Give the key to ProtoBoy"),
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							Paragraph(LOCTEXT("Step4Body", "Paste it into Project Settings > Plugins > ProtoBoy > Tasks > Trello Api Key. It's saved to DefaultProtoBoy.ini, which is fine to commit: an API key isn't a secret."))
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Left)
						.Padding(0.f, 6.f, 0.f, 0.f)
						[
							SNew(SButton)
							.Text(LOCTEXT("OpenProjectSettings", "Open Project Settings..."))
							.OnClicked(this, &SProtoBoyTrelloPanel::OnOpenProjectSettingsClicked)
						])
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 12.f, 0.f, 0.f)
				[
					Paragraph(LOCTEXT("ThenConnect", "Then click Connect above."))
				]
			]
		];
}

FReply SProtoBoyTrelloPanel::OnConnectClicked()
{
	StartError = FText::GetEmpty();
	if (FProtoBoyTrelloConnector* Connector = GetConnector())
	{
		Connector->BeginConnect(StartError);
	}
	return FReply::Handled();
}

FReply SProtoBoyTrelloPanel::OnCancelClicked()
{
	if (FProtoBoyTrelloConnector* Connector = GetConnector())
	{
		Connector->CancelConnect();
	}
	return FReply::Handled();
}

FReply SProtoBoyTrelloPanel::OnDisconnectClicked()
{
	StartError = FText::GetEmpty();
	if (FProtoBoyTrelloConnector* Connector = GetConnector())
	{
		Connector->Disconnect();
	}
	return FReply::Handled();
}

FReply SProtoBoyTrelloPanel::OnCopyOriginClicked()
{
	FPlatformApplicationMisc::ClipboardCopy(*FProtoBoyTrelloConnector::GetCallbackOrigin());
	return FReply::Handled();
}

FReply SProtoBoyTrelloPanel::OnOpenProjectSettingsClicked()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->ShowViewer("Project", "Plugins", GetDefault<UProtoBoySettings>()->GetSectionName());
	}
	return FReply::Handled();
}

FText SProtoBoyTrelloPanel::GetStatusText() const
{
	if (!StartError.IsEmpty())
	{
		return StartError;
	}

	const FProtoBoyTrelloConnector* Connector = GetConnector();
	if (!Connector)
	{
		return FText::GetEmpty();
	}

	switch (Connector->GetState())
	{
	case FProtoBoyTrelloConnector::EState::WaitingForBrowser:
		return LOCTEXT("Waiting", "Waiting for you to click Allow in the browser...");
	case FProtoBoyTrelloConnector::EState::Verifying:
		return LOCTEXT("Verifying", "Checking with Trello...");
	case FProtoBoyTrelloConnector::EState::Idle:
		break;
	}

	if (Connector->IsConnected())
	{
		const FProtoBoyTasksConnection& Connection = Connector->GetConnection();
		return Connection.TrelloFullName.IsEmpty()
			? FText::Format(LOCTEXT("ConnectedUser", "Connected as @{0}"), FText::FromString(Connection.TrelloUsername))
			: FText::Format(LOCTEXT("ConnectedFull", "Connected as {0} (@{1})"),
				FText::FromString(Connection.TrelloFullName), FText::FromString(Connection.TrelloUsername));
	}

	if (!Connector->GetLastError().IsEmpty())
	{
		return Connector->GetLastError();
	}
	return LOCTEXT("NotConnected", "Not connected.");
}

FSlateColor SProtoBoyTrelloPanel::GetStatusColor() const
{
	const FProtoBoyTrelloConnector* Connector = GetConnector();
	if (!StartError.IsEmpty() || (Connector && Connector->GetState() == FProtoBoyTrelloConnector::EState::Idle && !Connector->IsConnected() && !Connector->GetLastError().IsEmpty()))
	{
		return FStyleColors::Error;
	}
	if (Connector && Connector->GetState() == FProtoBoyTrelloConnector::EState::Idle && Connector->IsConnected())
	{
		return FStyleColors::Success;
	}
	return FSlateColor::UseForeground();
}

FProtoBoyTrelloConnector* SProtoBoyTrelloPanel::GetConnector()
{
	UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	return Subsystem ? &Subsystem->GetTrelloConnector() : nullptr;
}

#undef LOCTEXT_NAMESPACE
