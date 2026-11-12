#include "UI/Planning/SPBPillarTreeEditor.h"

#include "Docs/Obsidian/PBObsidianConnector.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "PBSettings.h"
#include "PBSubsystem.h"
#include "Planning/PBPillars.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "UI/Planning/SPBPillarDocsPreview.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SProtoBoyPillarTreeEditor"

void SProtoBoyPillarTreeEditor::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Intro", "Define the game's architectural pillars: mechanics, domains, disciplines, whatever fits. Nest them as deep as you like. ProtoBoy generates a doc for each, and agents use the tree to keep their work aligned."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 8.f, 0.f, 0.f)
		[
			MakeToolbar()
		]

		// Tree
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 6.f, 0.f, 0.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
			.Padding(4.f)
			[
				SNew(SVerticalBox)

				// Empty state
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(8.f)
				[
					SNew(SVerticalBox)
					.Visibility_Lambda([]()
					{
						const FProtoBoyPillarService* Service = GetService();
						return Service && Service->GetPillars().IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
					})

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("EmptyTree", "No pillars yet. Add your own, or start from a common set and adapt it."))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.AutoWrapText(true)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Left)
					.Padding(0.f, 6.f, 0.f, 0.f)
					[
						SNew(SButton)
						.Text(LOCTEXT("Starter", "Start with Common Pillars"))
						.ToolTipText(LOCTEXT("StarterTip", "Adds Gameplay, Animation, Art, Audio, Content and UI. Rename, nest or remove them freely."))
						.OnClicked(this, &SProtoBoyPillarTreeEditor::OnStarterClicked)
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.MaxDesiredHeight(320.f)
					[
						SAssignNew(TreeView, STreeView<FNodePtr>)
						.TreeItemsSource(&Roots)
						.SelectionMode(ESelectionMode::Single)
						.OnGenerateRow(this, &SProtoBoyPillarTreeEditor::OnGenerateRow)
						.OnGetChildren(this, &SProtoBoyPillarTreeEditor::OnGetChildren)
						.OnSelectionChanged(this, &SProtoBoyPillarTreeEditor::OnSelectionChanged)
					]
				]
			]
		]

		// Details of the selected pillar
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 8.f, 0.f, 0.f)
		[
			SAssignNew(DetailsBox, SBox)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Visibility_Lambda([this]() { return EditError.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
			.Text_Lambda([this]() { return EditError; })
			.ColorAndOpacity(FStyleColors::Error)
			.AutoWrapText(true)
		]

		// Generate + where it's stored
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
				.Text_Lambda([]()
				{
					const FProtoBoyPillarService* Service = GetService();
					if (Service && !Service->GetLastError().IsEmpty())
					{
						return Service->GetLastError();
					}
					return FText::Format(LOCTEXT("StoredIn", "Saved to {0} (commit it with the project). Docs go to {1}/ in the docs folder."),
						FText::FromString(GetDefault<UProtoBoySettings>()->PillarsFilePath),
						FText::FromString(GetDefault<UProtoBoySettings>()->PillarDocsFolder));
				})
				.ColorAndOpacity_Lambda([]()
				{
					const FProtoBoyPillarService* Service = GetService();
					return Service && !Service->GetLastError().IsEmpty() ? FSlateColor(FStyleColors::Error) : FSlateColor::UseSubduedForeground();
				})
				.AutoWrapText(true)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
				.Text(LOCTEXT("GenerateDocs", "Generate Docs..."))
				.ToolTipText(LOCTEXT("GenerateDocsTip", "Preview and write a doc for each pillar (and the Architecture doc's pillar overview)."))
				.IsEnabled_Lambda([]()
				{
					const FProtoBoyPillarService* Service = GetService();
					return Service && !Service->GetPillars().IsEmpty();
				})
				.OnClicked(this, &SProtoBoyPillarTreeEditor::OnGenerateDocsClicked)
			]
		]
	];

	if (FProtoBoyPillarService* Service = GetService())
	{
		ChangedHandle = Service->OnChanged.AddSP(this, &SProtoBoyPillarTreeEditor::RebuildTree);
	}
	RebuildTree();
}

SProtoBoyPillarTreeEditor::~SProtoBoyPillarTreeEditor()
{
	if (FProtoBoyPillarService* Service = GetService())
	{
		Service->OnChanged.Remove(ChangedHandle);
	}
}

TSharedRef<SWidget> SProtoBoyPillarTreeEditor::MakeToolbar()
{
	auto HasSelection = [this]() { const FProtoBoyPillarService* Service = GetService(); return Service && Service->Find(SelectedId); };

	return SNew(SWrapBox)
		.UseAllottedSize(true)
		.InnerSlotPadding(FVector2D(4.f, 4.f))

		+ SWrapBox::Slot()
		[
			SNew(SButton)
			.Text(LOCTEXT("AddPillar", "Add Pillar"))
			.ToolTipText(LOCTEXT("AddPillarTip", "Add a top-level pillar."))
			.OnClicked(this, &SProtoBoyPillarTreeEditor::OnAddClicked, false)
		]

		+ SWrapBox::Slot()
		[
			SNew(SButton)
			.Text(LOCTEXT("AddChild", "Add Child"))
			.ToolTipText(LOCTEXT("AddChildTip", "Add a pillar under the selected one."))
			.IsEnabled_Lambda(HasSelection)
			.OnClicked(this, &SProtoBoyPillarTreeEditor::OnAddClicked, true)
		]

		+ SWrapBox::Slot()
		[
			SNew(SButton)
			.Text(LOCTEXT("MoveUp", "Move Up"))
			.IsEnabled_Lambda(HasSelection)
			.OnClicked(this, &SProtoBoyPillarTreeEditor::OnMoveClicked, -1)
		]

		+ SWrapBox::Slot()
		[
			SNew(SButton)
			.Text(LOCTEXT("MoveDown", "Move Down"))
			.IsEnabled_Lambda(HasSelection)
			.OnClicked(this, &SProtoBoyPillarTreeEditor::OnMoveClicked, 1)
		]

		+ SWrapBox::Slot()
		[
			SNew(SButton)
			.Text(LOCTEXT("Remove", "Remove"))
			.ToolTipText(LOCTEXT("RemoveTip", "Remove the selected pillar and its children. Their docs are kept and marked archived on the next doc generation."))
			.IsEnabled_Lambda(HasSelection)
			.OnClicked(this, &SProtoBoyPillarTreeEditor::OnRemoveClicked)
		];
}

TSharedRef<ITableRow> SProtoBoyPillarTreeEditor::OnGenerateRow(FNodePtr Node, const TSharedRef<STableViewBase>& OwnerTable)
{
	const FString Id = Node->Id;
	return SNew(STableRow<FNodePtr>, OwnerTable)
		.Padding(FMargin(2.f, 3.f))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([Id]()
				{
					const FProtoBoyPillarService* Service = GetService();
					const FProtoBoyPillar* Pillar = Service ? Service->Find(Id) : nullptr;
					return Pillar ? FText::FromString(Pillar->Name) : FText::GetEmpty();
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text_Lambda([Id]()
				{
					const FProtoBoyPillarService* Service = GetService();
					const FProtoBoyPillar* Pillar = Service ? Service->Find(Id) : nullptr;
					return Pillar ? FText::FromString(Pillar->Kind) : FText::GetEmpty();
				})
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			.Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text_Lambda([Id]()
				{
					const FProtoBoyPillarService* Service = GetService();
					const FProtoBoyPillar* Pillar = Service ? Service->Find(Id) : nullptr;
					return Pillar ? FText::FromString(Pillar->Vision) : FText::GetEmpty();
				})
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			]
		];
}

void SProtoBoyPillarTreeEditor::OnGetChildren(FNodePtr Node, TArray<FNodePtr>& OutChildren)
{
	OutChildren = Node->Children;
}

void SProtoBoyPillarTreeEditor::OnSelectionChanged(FNodePtr Node, ESelectInfo::Type SelectInfo)
{
	if (bRebuilding)
	{
		return;
	}
	SelectedId = Node.IsValid() ? Node->Id : FString();
	EditError = FText::GetEmpty();
	RebuildDetails();
}

void SProtoBoyPillarTreeEditor::RebuildTree()
{
	const FProtoBoyPillarService* Service = GetService();
	Roots.Reset();
	if (!Service)
	{
		return;
	}

	TArray<FNodePtr> All;
	TFunction<void(const FString&, TArray<FNodePtr>&)> Build = [&](const FString& ParentId, TArray<FNodePtr>& Out)
	{
		for (const FProtoBoyPillar* Pillar : Service->GetChildren(ParentId))
		{
			FNodePtr Node = MakeShared<FNode>();
			Node->Id = Pillar->Id;
			Build(Pillar->Id, Node->Children);
			Out.Add(Node);
			All.Add(Node);
		}
	};
	Build(FString(), Roots);

	if (!TreeView.IsValid())
	{
		return;
	}

	bRebuilding = true;
	TreeView->RequestTreeRefresh();
	FNodePtr Selected;
	for (const FNodePtr& Node : All)
	{
		// The tree is small: keep it fully expanded so the whole architecture is visible.
		TreeView->SetItemExpansion(Node, true);
		if (Node->Id == SelectedId)
		{
			Selected = Node;
		}
	}
	if (Selected.IsValid())
	{
		TreeView->SetSelection(Selected);
	}
	else
	{
		TreeView->ClearSelection();
		SelectedId.Reset();
	}
	bRebuilding = false;

	ScheduleDetailsRebuild();
}

void SProtoBoyPillarTreeEditor::ScheduleDetailsRebuild()
{
	if (bDetailsRebuildPending)
	{
		return;
	}
	bDetailsRebuildPending = true;
	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda([this](double, float)
	{
		bDetailsRebuildPending = false;
		RebuildDetails();
		return EActiveTimerReturnType::Stop;
	}));
}

void SProtoBoyPillarTreeEditor::Select(const FString& Id)
{
	SelectedId = Id;
	RebuildTree();
}

TSharedRef<SWidget> SProtoBoyPillarTreeEditor::MakeLabel(const FText& Text)
{
	return SNew(SBox)
		.WidthOverride(90.f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(Text)
		];
}

void SProtoBoyPillarTreeEditor::RebuildDetails()
{
	if (!DetailsBox.IsValid())
	{
		return;
	}
	const FProtoBoyPillarService* Service = GetService();
	if (!Service || !Service->Find(SelectedId))
	{
		DetailsBox->SetContent(
			SNew(STextBlock)
			.Visibility_Lambda([]() { const FProtoBoyPillarService* S = GetService(); return S && !S->GetPillars().IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
			.Text(LOCTEXT("SelectPillar", "Select a pillar to edit it."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground()));
		return;
	}
	DetailsBox->SetContent(MakeDetails(SelectedId));
}

TSharedRef<SWidget> SProtoBoyPillarTreeEditor::MakeDetails(const FString& Id)
{
	const FProtoBoyPillarService* Service = GetService();
	const FProtoBoyPillar Pillar = *Service->Find(Id);

	// Applies one field edit to a copy of the pillar and saves it.
	auto Commit = [this, Id](TFunction<void(FProtoBoyPillar&)> Edit)
	{
		FProtoBoyPillarService* MutableService = GetService();
		const FProtoBoyPillar* Current = MutableService ? MutableService->Find(Id) : nullptr;
		if (!Current)
		{
			return;
		}
		FProtoBoyPillar Edited = *Current;
		Edit(Edited);
		EditError = FText::GetEmpty();
		MutableService->UpdatePillar(Edited, EditError);
	};

	// Parent options: top level, or any pillar outside this one's subtree.
	ParentOptions.Reset();
	ParentOptions.Add(MakeShared<FString>());
	TSharedPtr<FString> CurrentParent = ParentOptions[0];
	for (const FProtoBoyPillar& Other : Service->GetPillars())
	{
		if (!Service->IsInSubtree(Other.Id, Id))
		{
			TSharedPtr<FString> Option = MakeShared<FString>(Other.Id);
			ParentOptions.Add(Option);
			if (Other.Id == Pillar.ParentId)
			{
				CurrentParent = Option;
			}
		}
	}

	// Depends on: every other pillar as a checkbox.
	TSharedRef<SWrapBox> Dependencies = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(10.f, 2.f));
	for (const FProtoBoyPillar& Other : Service->GetPillars())
	{
		if (Other.Id == Id)
		{
			continue;
		}
		const FString OtherId = Other.Id;
		Dependencies->AddSlot()
		[
			SNew(SCheckBox)
			.IsChecked(Pillar.DependsOn.Contains(OtherId) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.OnCheckStateChanged_Lambda([Commit, OtherId](ECheckBoxState State)
			{
				Commit([OtherId, State](FProtoBoyPillar& Edited)
				{
					if (State == ECheckBoxState::Checked) { Edited.DependsOn.AddUnique(OtherId); }
					else { Edited.DependsOn.Remove(OtherId); }
				});
			})
			[
				SNew(STextBlock).Text(FText::FromString(Service->GetDisplayPath(OtherId)))
			]
		];
	}

	return SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		.Padding(10.f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("DetailsHeader", "{0}  ({1})"), FText::FromString(Service->GetDisplayPath(Id)), FText::FromString(Id)))
				.ToolTipText(LOCTEXT("IdTip", "The pillar's id stays the same when you rename or move it, so docs and tasks keep pointing at it."))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
			]

			// Name
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[ MakeLabel(LOCTEXT("NameLabel", "Name")) ]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SEditableTextBox)
					.Text(FText::FromString(Pillar.Name))
					.OnTextCommitted_Lambda([Commit](const FText& Text, ETextCommit::Type)
					{
						Commit([&Text](FProtoBoyPillar& Edited) { Edited.Name = Text.ToString(); });
					})
				]
			]

			// Kind
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[ MakeLabel(LOCTEXT("KindLabel", "Kind")) ]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SEditableTextBox)
					.Text(FText::FromString(Pillar.Kind))
					.HintText(LOCTEXT("KindHint", "Domain, Mechanic, Discipline, ..."))
					.OnTextCommitted_Lambda([Commit](const FText& Text, ETextCommit::Type)
					{
						Commit([&Text](FProtoBoyPillar& Edited) { Edited.Kind = Text.ToString(); });
					})
				]
			]

			// Parent
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[ MakeLabel(LOCTEXT("ParentLabel", "Parent")) ]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SComboBox<TSharedPtr<FString>>)
					.OptionsSource(&ParentOptions)
					.InitiallySelectedItem(CurrentParent)
					.OnGenerateWidget_Lambda([](TSharedPtr<FString> Option) -> TSharedRef<SWidget>
					{
						const FProtoBoyPillarService* S = GetService();
						return SNew(STextBlock).Text(Option->IsEmpty() || !S
							? LOCTEXT("TopLevel", "(top level)")
							: FText::FromString(S->GetDisplayPath(*Option)));
					})
					.OnSelectionChanged_Lambda([this, Id](TSharedPtr<FString> Option, ESelectInfo::Type SelectInfo)
					{
						if (!Option.IsValid() || SelectInfo == ESelectInfo::Direct)
						{
							return;
						}
						if (FProtoBoyPillarService* S = GetService())
						{
							EditError = FText::GetEmpty();
							S->SetParent(Id, *Option, EditError);
						}
					})
					[
						SNew(STextBlock)
						.Text_Lambda([Id]()
						{
							const FProtoBoyPillarService* S = GetService();
							const FProtoBoyPillar* P = S ? S->Find(Id) : nullptr;
							return !P || P->ParentId.IsEmpty() ? LOCTEXT("TopLevel", "(top level)") : FText::FromString(S->GetDisplayPath(P->ParentId));
						})
					]
				]
			]

			// Vision
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)[ MakeLabel(LOCTEXT("VisionLabel", "Vision")) ]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SMultiLineEditableTextBox)
					.Text(FText::FromString(Pillar.Vision))
					.HintText(LOCTEXT("VisionHint", "One or two sentences: what this pillar is for. The full vision goes in its doc."))
					.AutoWrapText(true)
					.OnTextCommitted_Lambda([Commit](const FText& Text, ETextCommit::Type)
					{
						Commit([&Text](FProtoBoyPillar& Edited) { Edited.Vision = Text.ToString(); });
					})
				]
			]

			// Depends on
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 6.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)[ MakeLabel(LOCTEXT("DependsLabel", "Depends on")) ]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					Dependencies
				]
			]

			// Doc
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 6.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[ MakeLabel(LOCTEXT("DocLabel", "Doc")) ]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(Pillar.DocPath.IsEmpty()
						? LOCTEXT("NoDoc", "Not generated yet. Use Generate Docs.")
						: FText::FromString(Pillar.DocPath))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(6.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.Visibility_Lambda([DocPath = Pillar.DocPath]()
					{
						const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
						return !DocPath.IsEmpty() && Subsystem && Subsystem->GetObsidianConnector().IsConnected() ? EVisibility::Visible : EVisibility::Collapsed;
					})
					.Text(LOCTEXT("OpenDoc", "Open in Obsidian"))
					.OnClicked_Lambda([this, DocPath = Pillar.DocPath]()
					{
						if (const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get())
						{
							EditError = FText::GetEmpty();
							Subsystem->GetObsidianConnector().OpenInObsidian(DocPath, EditError);
						}
						return FReply::Handled();
					})
				]
			]
		];
}

FReply SProtoBoyPillarTreeEditor::OnAddClicked(bool bAsChild)
{
	if (FProtoBoyPillarService* Service = GetService())
	{
		const FString ParentId = bAsChild ? SelectedId : FString();
		Select(Service->AddPillar(LOCTEXT("NewPillarName", "New Pillar").ToString(), ParentId));
	}
	return FReply::Handled();
}

FReply SProtoBoyPillarTreeEditor::OnMoveClicked(int32 Delta)
{
	if (FProtoBoyPillarService* Service = GetService())
	{
		Service->MoveAmongSiblings(SelectedId, Delta);
	}
	return FReply::Handled();
}

FReply SProtoBoyPillarTreeEditor::OnRemoveClicked()
{
	FProtoBoyPillarService* Service = GetService();
	const FProtoBoyPillar* Pillar = Service ? Service->Find(SelectedId) : nullptr;
	if (!Pillar)
	{
		return FReply::Handled();
	}

	int32 Descendants = 0;
	for (const FProtoBoyPillar& Other : Service->GetPillars())
	{
		Descendants += Other.Id != Pillar->Id && Service->IsInSubtree(Other.Id, Pillar->Id) ? 1 : 0;
	}

	const FText Message = Descendants > 0
		? FText::Format(LOCTEXT("ConfirmRemoveTree", "Remove \"{0}\" and the {1} {1}|plural(one=pillar,other=pillars) under it?\n\nTheir docs are kept and marked archived the next time you generate docs."),
			FText::FromString(Pillar->Name), FText::AsNumber(Descendants))
		: FText::Format(LOCTEXT("ConfirmRemove", "Remove \"{0}\"?\n\nIts doc is kept and marked archived the next time you generate docs."), FText::FromString(Pillar->Name));

	if (FMessageDialog::Open(EAppMsgType::YesNo, Message, LOCTEXT("ConfirmRemoveTitle", "Remove Pillar")) == EAppReturnType::Yes)
	{
		Service->RemovePillar(SelectedId);
	}
	return FReply::Handled();
}

FReply SProtoBoyPillarTreeEditor::OnStarterClicked()
{
	if (FProtoBoyPillarService* Service = GetService())
	{
		Service->AddStarterPillars();
	}
	return FReply::Handled();
}

FReply SProtoBoyPillarTreeEditor::OnGenerateDocsClicked()
{
	SProtoBoyPillarDocsPreview::Open(AsShared());
	return FReply::Handled();
}

FProtoBoyPillarService* SProtoBoyPillarTreeEditor::GetService()
{
	UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	return Subsystem ? &Subsystem->GetPillars() : nullptr;
}

#undef LOCTEXT_NAMESPACE
