#include "UI/Architecture/SPBPillarTreeEditor.h"

#include "Docs/Obsidian/PBObsidianConnector.h"
#include "Docs/PBDocGenerator.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "PBSettings.h"
#include "PBSubsystem.h"
#include "Architecture/PBPillars.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "UI/Architecture/PBPillarGraph.h"
#include "UI/Architecture/SPBPillarDocsPreview.h"
#include "UI/Architecture/SPBPillarGraphNode.h"
#include "UI/Architecture/SPBSetupPillarsDialog.h"
#include "UObject/Package.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SProtoBoyPillarTreeEditor"

namespace ProtoBoyPillarTreeLayout
{
	/** Horizontal space per leaf, and vertical space per tree level. */
	constexpr float ColumnWidth = SProtoBoyPillarGraphNode::NodeWidth + 40.f;
	constexpr float LevelHeight = 190.f;
	constexpr float GraphHeight = 480.f;
}

void SProtoBoyPillarTreeEditor::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)

		// Setup Pillars / Clear Pillars
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.Padding(0.f, 0.f, 0.f, 8.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("SetupPillars", "Setup Pillars"))
				.OnClicked(this, &SProtoBoyPillarTreeEditor::OnSetupPillarsClicked)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(6.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("ClearPillars", "Clear Pillars"))
				.ToolTipText(LOCTEXT("ClearPillarsTip", "Remove every pillar below the game, leaving just the root. Their docs are kept and marked archived the next time you generate docs."))
				.IsEnabled_Lambda([]()
				{
					const FProtoBoyPillarService* Service = GetService();
					return Service && Service->HasPillars();
				})
				.OnClicked(this, &SProtoBoyPillarTreeEditor::OnClearPillarsClicked)
			]
		]

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

		// Graph
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 6.f, 0.f, 0.f)
		[
			MakeGraph()
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("GraphHelp", "Wire a pillar's bottom bar to another's top bar to nest it (to the root to make it top-level). Drag off a bottom bar into empty space to add a child. Right-click for more; mouse wheel zooms, right-drag pans."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
			.AutoWrapText(true)
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
					return Service && Service->HasPillars();
				})
				.OnClicked(this, &SProtoBoyPillarTreeEditor::OnGenerateDocsClicked)
			]
		]
	];

	if (FProtoBoyPillarService* Service = GetService())
	{
		ChangedHandle = Service->OnChanged.AddSP(this, &SProtoBoyPillarTreeEditor::ScheduleRebuild);
	}
	RebuildGraph();
	RebuildDetails();
	ZoomToFitSoon();
}

SProtoBoyPillarTreeEditor::~SProtoBoyPillarTreeEditor()
{
	if (FProtoBoyPillarService* Service = GetService())
	{
		Service->OnChanged.Remove(ChangedHandle);
	}
}

void SProtoBoyPillarTreeEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	// The graph owns its nodes (UEdGraph::Nodes), so referencing it keeps RootNode and NodesById alive too.
	Collector.AddReferencedObject(Graph);
}

TSharedRef<SWidget> SProtoBoyPillarTreeEditor::MakeGraph()
{
	Graph = NewObject<UProtoBoyPillarGraph>(GetTransientPackage(), NAME_None, RF_Transient);
	Graph->Schema = UProtoBoyPillarGraphSchema::StaticClass();
	Graph->Editor = SharedThis(this);

	GraphCommands = MakeShared<FUICommandList>();
	GraphCommands->MapAction(FGenericCommands::Get().Delete,
		FExecuteAction::CreateSPLambda(this, [this]() { OnRemoveClicked(); }),
		FCanExecuteAction::CreateSP(this, &SProtoBoyPillarTreeEditor::HasSelectedPillar));
	GraphCommands->MapAction(FGenericCommands::Get().Rename,
		FExecuteAction::CreateSPLambda(this, [this]() { RequestRename(SelectedId); }),
		FCanExecuteAction::CreateSP(this, &SProtoBoyPillarTreeEditor::HasSelectedPillar));

	SGraphEditor::FGraphEditorEvents Events;
	Events.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &SProtoBoyPillarTreeEditor::OnGraphSelectionChanged);
	Events.OnTextCommitted = FOnNodeTextCommitted::CreateSP(this, &SProtoBoyPillarTreeEditor::OnNodeTextCommitted);
	Events.OnVerifyTextCommit = FOnNodeVerifyTextCommit::CreateSP(this, &SProtoBoyPillarTreeEditor::OnNodeVerifyTextCommit);
	Events.OnNodeDoubleClicked = FSingleNodeEvent::CreateSP(this, &SProtoBoyPillarTreeEditor::OnNodeDoubleClicked);
	Events.OnCreateActionMenuAtLocation = SGraphEditor::FOnCreateActionMenuAtLocation::CreateSP(this, &SProtoBoyPillarTreeEditor::OnCreateActionMenu);
	Events.OnCreateNodeOrPinMenu = SGraphEditor::FOnCreateNodeOrPinMenu::CreateSP(this, &SProtoBoyPillarTreeEditor::OnCreateNodeOrPinMenu);

	FGraphAppearanceInfo Appearance;
	Appearance.CornerText = LOCTEXT("CornerText", "PILLARS");

	return SNew(SBox)
		.HeightOverride(ProtoBoyPillarTreeLayout::GraphHeight)
		[
			SAssignNew(GraphEditor, SGraphEditor)
			.GraphToEdit(Graph)
			.AdditionalCommands(GraphCommands)
			.Appearance(Appearance)
			.GraphEvents(Events)
			.ShowGraphStateOverlay(false)
		];
}

TSharedRef<SWidget> SProtoBoyPillarTreeEditor::MakeToolbar()
{
	auto HasSelection = [this]() { return HasSelectedPillar(); };

	auto IconButton = [](const FName& Icon, const FText& ToolTip, TFunction<bool()> IsEnabled, FOnClicked OnClicked) -> TSharedRef<SWidget>
	{
		return SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ContentPadding(FMargin(4.f, 2.f))
			.ToolTipText(ToolTip)
			.IsEnabled_Lambda([IsEnabled]() { return IsEnabled(); })
			.OnClicked(OnClicked)
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush(Icon))
				.ColorAndOpacity(FSlateColor::UseForeground())
			];
	};

	return SNew(SHorizontalBox)

		// + Add
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SComboButton)
			.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("SimpleComboButton"))
			.ToolTipText(LOCTEXT("AddTip", "Add a pillar."))
			.OnGetMenuContent(this, &SProtoBoyPillarTreeEditor::MakeAddMenu)
			.ButtonContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.Plus"))
					.ColorAndOpacity(FStyleColors::AccentGreen)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4.f, 0.f, 0.f, 0.f)
				[
					SNew(STextBlock).Text(LOCTEXT("Add", "Add"))
				]
			]
		]

		// Search: dims pillars that don't match; Enter jumps to the first match.
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Center)
		.Padding(6.f, 0.f)
		[
			SNew(SSearchBox)
			.HintText(LOCTEXT("SearchHint", "Search pillars"))
			.OnTextChanged(this, &SProtoBoyPillarTreeEditor::OnFilterTextChanged)
			.OnTextCommitted(this, &SProtoBoyPillarTreeEditor::OnFilterTextCommitted)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			IconButton("Icons.ArrowLeft", LOCTEXT("MoveLeftTip", "Move the selected pillar left among its siblings."), HasSelection,
				FOnClicked::CreateSP(this, &SProtoBoyPillarTreeEditor::OnMoveClicked, -1))
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			IconButton("Icons.ArrowRight", LOCTEXT("MoveRightTip", "Move the selected pillar right among its siblings."), HasSelection,
				FOnClicked::CreateSP(this, &SProtoBoyPillarTreeEditor::OnMoveClicked, 1))
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			IconButton("Icons.Delete", LOCTEXT("RemoveTip", "Remove the selected pillar and its children (Delete). Their docs are kept and marked archived on the next doc generation."), HasSelection,
				FOnClicked::CreateSP(this, &SProtoBoyPillarTreeEditor::OnRemoveClicked))
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			IconButton("Icons.Fullscreen", LOCTEXT("ZoomToFitTip", "Zoom to fit the whole tree."), []() { return true; },
				FOnClicked::CreateSPLambda(this, [this]()
				{
					if (GraphEditor.IsValid())
					{
						GraphEditor->ZoomToFit(false);
					}
					return FReply::Handled();
				}))
		];
}

TSharedRef<SWidget> SProtoBoyPillarTreeEditor::MakeAddMenu()
{
	const FProtoBoyPillarService* Service = GetService();
	const bool bHasSelection = Service && Service->Find(SelectedId);

	FMenuBuilder Menu(true, nullptr);
	Menu.AddMenuEntry(
		LOCTEXT("AddPillar", "Pillar"),
		LOCTEXT("AddPillarTip", "Add a top-level pillar (directly under the project)."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.FolderClosed"),
		FUIAction(FExecuteAction::CreateSPLambda(this, [this]() { OnAddClicked(false); })));
	Menu.AddMenuEntry(
		LOCTEXT("AddChild", "Child Pillar"),
		bHasSelection
			? FText::Format(LOCTEXT("AddChildTipNamed", "Add a pillar under {0}."), FText::FromString(Service->GetDisplayPath(SelectedId)))
			: LOCTEXT("AddChildTip", "Select a pillar first."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.FolderClosed"),
		FUIAction(
			FExecuteAction::CreateSPLambda(this, [this]() { OnAddClicked(true); }),
			FCanExecuteAction::CreateLambda([bHasSelection]() { return bHasSelection; })));
	return Menu.MakeWidget();
}

// --- Graph events -------------------------------------------------------------------------------------------------

void SProtoBoyPillarTreeEditor::OnGraphSelectionChanged(const TSet<UObject*>& Selection)
{
	if (bRebuilding)
	{
		return;
	}
	FString NewSelection;
	for (UObject* Object : Selection)
	{
		if (const UProtoBoyPillarGraphNode* Node = Cast<UProtoBoyPillarGraphNode>(Object))
		{
			NewSelection = Node->PillarId;
			break;
		}
	}
	if (NewSelection != SelectedId)
	{
		SelectedId = NewSelection;
		EditError = FText::GetEmpty();
		RebuildDetails();
	}
}

bool SProtoBoyPillarTreeEditor::OnNodeVerifyTextCommit(const FText& Text, UEdGraphNode* Node, FText& OutError)
{
	if (Text.ToString().TrimStartAndEnd().IsEmpty())
	{
		OutError = LOCTEXT("EmptyNameInline", "A pillar needs a name.");
		return false;
	}
	return true;
}

void SProtoBoyPillarTreeEditor::OnNodeTextCommitted(const FText& Text, ETextCommit::Type CommitType, UEdGraphNode* Node)
{
	const UProtoBoyPillarGraphNode* PillarNode = Cast<UProtoBoyPillarGraphNode>(Node);
	FProtoBoyPillarService* Service = GetService();
	const FProtoBoyPillar* Current = PillarNode && Service ? Service->Find(PillarNode->PillarId) : nullptr;
	if (!Current || CommitType == ETextCommit::OnCleared || Current->Name == Text.ToString())
	{
		return;
	}
	FProtoBoyPillar Edited = *Current;
	Edited.Name = Text.ToString();
	EditError = FText::GetEmpty();
	Service->UpdatePillar(Edited, EditError);
}

void SProtoBoyPillarTreeEditor::OnNodeDoubleClicked(UEdGraphNode* Node)
{
	// Like opening a Behavior Tree task: open the pillar's doc if there is one, else rename it.
	const UProtoBoyPillarGraphNode* PillarNode = Cast<UProtoBoyPillarGraphNode>(Node);
	const FProtoBoyPillarService* Service = GetService();
	const FProtoBoyPillar* Pillar = PillarNode && Service ? Service->Find(PillarNode->PillarId) : nullptr;
	if (!Pillar)
	{
		return;
	}
	const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	const FString DocPath = Pillar->bRoot ? FProtoBoyDocGenerator::GetArchitectureDocPath() : Pillar->DocPath;
	if (!DocPath.IsEmpty() && Subsystem && Subsystem->GetObsidianConnector().IsConnected())
	{
		EditError = FText::GetEmpty();
		Subsystem->GetObsidianConnector().OpenInObsidian(DocPath, EditError);
		return;
	}
	RequestRename(Pillar->Id);
}

FActionMenuContent SProtoBoyPillarTreeEditor::OnCreateActionMenu(UEdGraph* InGraph, const FVector2f& Location, const TArray<UEdGraphPin*>& DraggedPins,
	bool bAutoExpand, SGraphEditor::FActionMenuClosed OnClosed)
{
	// Dragged off a node's bottom bar: offer a child of that node. Otherwise a top-level pillar.
	FString ParentId;
	bool bFromPin = false;
	for (const UEdGraphPin* Pin : DraggedPins)
	{
		const UProtoBoyPillarGraphNode* Node = Pin ? Cast<UProtoBoyPillarGraphNode>(Pin->GetOwningNode()) : nullptr;
		if (Node && Pin->Direction == EGPD_Output)
		{
			ParentId = Node->PillarId;
			bFromPin = true;
			break;
		}
	}

	const FProtoBoyPillarService* Service = GetService();
	FMenuBuilder Menu(true, nullptr);
	Menu.BeginSection("Add", LOCTEXT("AddSection", "Add"));
	Menu.AddMenuEntry(
		bFromPin && !ParentId.IsEmpty() && Service
			? FText::Format(LOCTEXT("ActionAddChild", "New Pillar under {0}"), FText::FromString(Service->GetDisplayPath(ParentId)))
			: LOCTEXT("ActionAddTop", "New Top-Level Pillar"),
		FText::GetEmpty(),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"),
		FUIAction(FExecuteAction::CreateSPLambda(this, [this, ParentId, OnClosed]()
		{
			OnClosed.ExecuteIfBound();
			AddPillarUnder(ParentId);
		})));
	Menu.EndSection();

	FActionMenuContent Content(Menu.MakeWidget());
	Content.OnMenuDismissed.AddLambda([OnClosed]() { OnClosed.ExecuteIfBound(); });
	return Content;
}

FActionMenuContent SProtoBoyPillarTreeEditor::OnCreateNodeOrPinMenu(UEdGraph* InGraph, const UEdGraphNode* Node, const UEdGraphPin* Pin, FMenuBuilder* Menu, bool bReadOnly)
{
	const UProtoBoyPillarGraphNode* PillarNode = Cast<UProtoBoyPillarGraphNode>(Node);
	FProtoBoyPillarService* Service = GetService();
	if (!PillarNode || !Service || !Menu)
	{
		return FActionMenuContent();
	}
	const FString Id = PillarNode->PillarId;

	if (PillarNode->IsRoot())
	{
		Menu->BeginSection("Root", PillarNode->GetNodeTitle(ENodeTitleType::FullTitle));
		Menu->AddMenuEntry(
			LOCTEXT("CtxAddPillar", "Add Pillar"),
			FText::GetEmpty(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"),
			FUIAction(FExecuteAction::CreateSPLambda(this, [this]() { AddPillarUnder(FString()); })));
		Menu->EndSection();
		return FActionMenuContent(Menu->MakeWidget());
	}

	// The menu's actions (and Rename/Delete commands) act on the selected pillar.
	if (SelectedId != Id)
	{
		SelectedId = Id;
		EditError = FText::GetEmpty();
		RebuildDetails();
	}

	Menu->BeginSection("Pillar", FText::FromString(Service->GetDisplayPath(Id)));
	Menu->AddMenuEntry(
		LOCTEXT("CtxAddChild", "Add Child"),
		LOCTEXT("CtxAddChildTip", "Add a pillar under this one."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"),
		FUIAction(FExecuteAction::CreateSPLambda(this, [this, Id]() { AddPillarUnder(Id); })));
	Menu->AddMenuEntry(FGenericCommands::Get().Rename);
	Menu->AddMenuEntry(
		LOCTEXT("CtxMoveLeft", "Move Left"),
		LOCTEXT("CtxMoveLeftTip", "Move before the previous sibling."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.ArrowLeft"),
		FUIAction(FExecuteAction::CreateSPLambda(this, [this]() { OnMoveClicked(-1); })));
	Menu->AddMenuEntry(
		LOCTEXT("CtxMoveRight", "Move Right"),
		LOCTEXT("CtxMoveRightTip", "Move after the next sibling."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.ArrowRight"),
		FUIAction(FExecuteAction::CreateSPLambda(this, [this]() { OnMoveClicked(1); })));
	const FProtoBoyPillar* Pillar = Service->Find(Id);
	if (Pillar && !Service->IsRoot(Pillar->ParentId))
	{
		Menu->AddMenuEntry(
			LOCTEXT("CtxMoveTop", "Move to Top Level"),
			LOCTEXT("CtxMoveTopTip", "Connect this pillar to the root."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSPLambda(this, [this, Id]() { ReparentFromGraph(Id, FString()); })));
	}
	Menu->AddMenuSeparator();
	Menu->AddMenuEntry(FGenericCommands::Get().Delete);
	Menu->EndSection();

	return FActionMenuContent(Menu->MakeWidget());
}

void SProtoBoyPillarTreeEditor::ReparentFromGraph(const FString& Id, const FString& NewParentId)
{
	if (FProtoBoyPillarService* Service = GetService())
	{
		SelectedId = Id;
		EditError = FText::GetEmpty();
		Service->SetParent(Id, NewParentId, EditError);
	}
}

bool SProtoBoyPillarTreeEditor::MatchesFilter(const FString& Id) const
{
	if (FilterText.IsEmpty())
	{
		return true;
	}
	const FProtoBoyPillarService* Service = GetService();
	const FProtoBoyPillar* Pillar = Service ? Service->Find(Id) : nullptr;
	const FString Filter = FilterText.ToString();
	return Pillar && (Pillar->Name.Contains(Filter) || Pillar->Kind.Contains(Filter) || Pillar->Vision.Contains(Filter));
}

bool SProtoBoyPillarTreeEditor::ConsumePendingRename(const FString& Id)
{
	if (!Id.IsEmpty() && PendingRenameId == Id)
	{
		PendingRenameId.Reset();
		return true;
	}
	return false;
}

void SProtoBoyPillarTreeEditor::OnFilterTextChanged(const FText& Text)
{
	// Node widgets read the filter every frame, so there's nothing to rebuild.
	FilterText = FText::TrimPrecedingAndTrailing(Text);
}

void SProtoBoyPillarTreeEditor::OnFilterTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	OnFilterTextChanged(Text);
	if (CommitType != ETextCommit::OnEnter || FilterText.IsEmpty() || !GraphEditor.IsValid())
	{
		return;
	}
	const FProtoBoyPillarService* Service = GetService();
	if (!Service)
	{
		return;
	}
	for (const FProtoBoyPillar& Pillar : Service->GetPillars())
	{
		if (MatchesFilter(Pillar.Id))
		{
			if (const TObjectPtr<UProtoBoyPillarGraphNode>* Node = NodesById.Find(Pillar.Id))
			{
				GraphEditor->JumpToNode(*Node, false, true);
			}
			return;
		}
	}
}

// --- Rebuild ------------------------------------------------------------------------------------------------------

void SProtoBoyPillarTreeEditor::ScheduleRebuild()
{
	// Deferred: edits arrive from inside graph callbacks (rename commit, wire drop), and rebuilding the graph there
	// would destroy the node widget that is still running the callback.
	if (bRebuildPending)
	{
		return;
	}
	bRebuildPending = true;
	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda([this](double, float)
	{
		bRebuildPending = false;
		RebuildGraph();
		RebuildDetails();
		return EActiveTimerReturnType::Stop;
	}));
}

void SProtoBoyPillarTreeEditor::RebuildGraph()
{
	const FProtoBoyPillarService* Service = GetService();
	if (!Graph || !Service)
	{
		return;
	}

	auto MakeNode = [this, Service](const FString& PillarId)
	{
		UProtoBoyPillarGraphNode* Node = NewObject<UProtoBoyPillarGraphNode>(Graph, NAME_None, RF_Transient);
		Node->PillarId = PillarId;
		Node->bRootNode = Service->IsRoot(PillarId);
		Node->bCanRenameNode = !Node->bRootNode;
		Node->CreateNewGuid();
		Node->AllocateDefaultPins();
		Graph->AddNode(Node, false, false);
		return Node;
	};

	// Sync nodes with pillars (the root included).
	TSet<FString> Live;
	for (const FProtoBoyPillar& Pillar : Service->GetPillars())
	{
		Live.Add(Pillar.Id);
		if (!NodesById.Contains(Pillar.Id))
		{
			NodesById.Add(Pillar.Id, MakeNode(Pillar.Id));
		}
	}
	for (auto It = NodesById.CreateIterator(); It; ++It)
	{
		if (!Live.Contains(It.Key()))
		{
			Graph->RemoveNode(It.Value());
			It.RemoveCurrent();
		}
	}
	RootNode = NodesById.FindRef(Service->GetRootId());
	if (!RootNode)
	{
		return;
	}

	// Relink parent -> children in sibling order, and lay the tree out top-down (each leaf gets a column; a parent
	// sits centered over its children), the way Behavior Trees read.
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			Pin->BreakAllPinLinks();
		}
	}

	TMap<FString, int32> Leaves;
	TFunction<int32(const FString&)> CountLeaves = [&](const FString& Id) -> int32
	{
		int32 Count = 0;
		for (const FProtoBoyPillar* Child : Service->GetChildren(Id))
		{
			Count += CountLeaves(Child->Id);
		}
		Count = FMath::Max(1, Count);
		Leaves.Add(Id, Count);
		return Count;
	};
	CountLeaves(FString());

	TFunction<void(UProtoBoyPillarGraphNode*, int32, float)> Place = [&](UProtoBoyPillarGraphNode* Node, int32 Depth, float Left)
	{
		const float Width = Leaves.FindRef(Node->PillarId) * ProtoBoyPillarTreeLayout::ColumnWidth;
		Node->NodePosX = FMath::RoundToInt(Left + (Width - SProtoBoyPillarGraphNode::NodeWidth) * 0.5f);
		Node->NodePosY = FMath::RoundToInt(Depth * ProtoBoyPillarTreeLayout::LevelHeight);

		float ChildLeft = Left;
		for (const FProtoBoyPillar* Child : Service->GetChildren(Node->PillarId))
		{
			UProtoBoyPillarGraphNode* ChildNode = NodesById.FindRef(Child->Id);
			if (!ChildNode)
			{
				continue;
			}
			Node->GetOutputPin()->MakeLinkTo(ChildNode->GetInputPin());
			Place(ChildNode, Depth + 1, ChildLeft);
			ChildLeft += Leaves.FindRef(Child->Id) * ProtoBoyPillarTreeLayout::ColumnWidth;
		}
	};
	Place(RootNode, 0, 0.f);

	if (!Service->Find(SelectedId))
	{
		SelectedId.Reset();
	}

	Graph->NotifyGraphChanged();

	if (GraphEditor.IsValid())
	{
		bRebuilding = true;
		GraphEditor->ClearSelectionSet();
		if (const TObjectPtr<UProtoBoyPillarGraphNode>* Selected = NodesById.Find(SelectedId))
		{
			GraphEditor->SetNodeSelection(*Selected, true);
		}
		bRebuilding = false;
	}
}

void SProtoBoyPillarTreeEditor::ZoomToFitSoon()
{
	// Node widgets are created on the panel's next update; fit once they have a size.
	RegisterActiveTimer(0.1f, FWidgetActiveTimerDelegate::CreateLambda([this](double, float)
	{
		if (GraphEditor.IsValid())
		{
			GraphEditor->ZoomToFit(false);
		}
		return EActiveTimerReturnType::Stop;
	}));
}

void SProtoBoyPillarTreeEditor::Select(const FString& Id)
{
	SelectedId = Id;
	EditError = FText::GetEmpty();
	ScheduleRebuild();
}

void SProtoBoyPillarTreeEditor::RequestRename(const FString& Id)
{
	if (const TObjectPtr<UProtoBoyPillarGraphNode>* Node = NodesById.Find(Id); Node && GraphEditor.IsValid())
	{
		GraphEditor->JumpToNode(*Node, true, true);
	}
}

bool SProtoBoyPillarTreeEditor::HasSelectedPillar() const
{
	// Rename, move and remove: every pillar but the root.
	const FProtoBoyPillarService* Service = GetService();
	return Service && Service->Find(SelectedId) && !Service->IsRoot(SelectedId);
}

void SProtoBoyPillarTreeEditor::AddPillarUnder(const FString& ParentId)
{
	if (FProtoBoyPillarService* Service = GetService())
	{
		// The add schedules a rebuild; the new node's widget sees PendingRenameId and opens in rename mode.
		const FString NewId = Service->AddPillar(LOCTEXT("NewPillarName", "New Pillar").ToString(), ParentId);
		PendingRenameId = NewId;
		Select(NewId);
	}
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
			.Visibility_Lambda([]() { const FProtoBoyPillarService* S = GetService(); return S && S->HasPillars() ? EVisibility::Visible : EVisibility::Collapsed; })
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

	// The root is the game: fixed name, no parent, no dependencies, and the Architecture doc as its doc.
	const bool bRoot = Pillar.bRoot;
	const FString DocPath = bRoot ? FProtoBoyDocGenerator::GetArchitectureDocPath() : Pillar.DocPath;

	// Parent options: any pillar outside this one's subtree (the root, the project, is the top level).
	ParentOptions.Reset();
	TSharedPtr<FString> CurrentParent;
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

	// Depends on: every other pillar (not the root) as a checkbox.
	TSharedRef<SWrapBox> Dependencies = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(10.f, 2.f));
	for (const FProtoBoyPillar& Other : Service->GetPillars())
	{
		if (Other.Id == Id || Other.bRoot)
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
					.IsReadOnly(bRoot)
					.ToolTipText(bRoot ? LOCTEXT("RootNameTip", "The root is named after the project.") : FText::GetEmpty())
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
				.Visibility(bRoot ? EVisibility::Collapsed : EVisibility::Visible)
				+ SHorizontalBox::Slot().AutoWidth()[ MakeLabel(LOCTEXT("ParentLabel", "Parent")) ]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SComboBox<TSharedPtr<FString>>)
					.OptionsSource(&ParentOptions)
					.InitiallySelectedItem(CurrentParent)
					.OnGenerateWidget_Lambda([](TSharedPtr<FString> Option) -> TSharedRef<SWidget>
					{
						const FProtoBoyPillarService* S = GetService();
						return SNew(STextBlock).Text(S ? FText::FromString(S->GetDisplayPath(*Option)) : FText::GetEmpty());
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
							return P ? FText::FromString(S->GetDisplayPath(P->ParentId)) : FText::GetEmpty();
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
				.Visibility(bRoot ? EVisibility::Collapsed : EVisibility::Visible)
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
					.Text(DocPath.IsEmpty()
						? LOCTEXT("NoDoc", "Not generated yet. Use Generate Docs.")
						: bRoot ? FText::Format(LOCTEXT("RootDoc", "{0} (the Architecture doc)"), FText::FromString(DocPath)) : FText::FromString(DocPath))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(6.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.Visibility_Lambda([DocPath]()
					{
						const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
						return !DocPath.IsEmpty() && Subsystem && Subsystem->GetObsidianConnector().IsConnected() ? EVisibility::Visible : EVisibility::Collapsed;
					})
					.Text(LOCTEXT("OpenDoc", "Open in Obsidian"))
					.OnClicked_Lambda([this, DocPath]()
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
	AddPillarUnder(bAsChild ? SelectedId : FString());
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

FReply SProtoBoyPillarTreeEditor::OnSetupPillarsClicked()
{
	SProtoBoySetupPillarsDialog::Open(AsShared());
	return FReply::Handled();
}

FReply SProtoBoyPillarTreeEditor::OnClearPillarsClicked()
{
	FProtoBoyPillarService* Service = GetService();
	if (!Service || !Service->HasPillars())
	{
		return FReply::Handled();
	}

	const int32 Count = Service->GetPillars().Num() - 1;
	const FText Message = FText::Format(
		LOCTEXT("ConfirmClear", "Remove all {0} {0}|plural(one=pillar,other=pillars) below \"{1}\"? Only the game itself stays.\n\nTheir docs are kept and marked archived the next time you generate docs."),
		FText::AsNumber(Count), FText::FromString(Service->GetRoot().Name));

	if (FMessageDialog::Open(EAppMsgType::YesNo, Message, LOCTEXT("ConfirmClearTitle", "Clear Pillars")) == EAppReturnType::Yes)
	{
		SelectedId = Service->GetRootId();
		EditError = FText::GetEmpty();
		Service->ClearPillars();
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
