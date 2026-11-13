#include "UI/Docs/Map/SPBDocMapEditor.h"

#include "Docs/Obsidian/PBObsidianConnector.h"
#include "Docs/PBDocGenerator.h"
#include "Docs/PBDocMap.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "PBSubsystem.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "UI/Docs/Map/PBDocMapGraph.h"
#include "UI/Docs/Map/SPBSetupDocsDialog.h"
#include "UObject/Package.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SProtoBoyDocMapEditor"

namespace ProtoBoyDocMapEditor
{
	constexpr float GraphHeight = 480.f;
	/** Longest Map Existing Doc menu before it says "and N more". */
	constexpr int32 MaxUnmappedInMenu = 40;

	/** "Design Doc", "Shared Tech Doc", "Content Doc", "Style Guide". */
	FText GetAddLabel(EProtoBoyDocKind Kind)
	{
		return Kind == EProtoBoyDocKind::StyleGuide
			? ProtoBoyDocLayers::GetKindName(Kind)
			: FText::Format(NSLOCTEXT("SProtoBoyDocMapEditor", "AddKind", "{0} Doc"), ProtoBoyDocLayers::GetKindName(Kind));
	}
}

void SProtoBoyDocMapEditor::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Intro", "The project's docs in four layers: the Game doc (why the game exists and what it is), Design docs (each feature, from the player's side), Tech docs (how things are built) and Content docs (what's in the game and how it looks, sounds and reads). Each doc's place is stored in its own frontmatter, so the docs are the source of truth. Agents read down the layers before they build."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		]

		// No Game doc yet
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 8.f, 0.f, 0.f)
		[
			MakeSetupRow()
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
			.Text(LOCTEXT("GraphHelp", "Wire a doc's bottom bar to another doc's top bar to put the second under the first. Drag off a bottom bar into empty space to add a doc there. Double-click opens a doc; right-click for more."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
			.AutoWrapText(true)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Visibility_Lambda([this]() { return StatusText.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
			.Text_Lambda([this]() { return StatusText; })
			.ColorAndOpacity_Lambda([this]() { return bStatusIsError ? FSlateColor(FStyleColors::Error) : FSlateColor(FStyleColors::Success); })
			.AutoWrapText(true)
		]

		// Details of the selected doc
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 8.f, 0.f, 0.f)
		[
			SAssignNew(DetailsBox, SBox)
		]
	];

	if (FProtoBoyDocMap* Map = GetMap())
	{
		ChangedHandle = Map->OnChanged.AddSP(this, &SProtoBoyDocMapEditor::ScheduleRebuild);
		// Pick up edits made outside the editor since the map was last read.
		Map->Refresh();
	}
	RebuildGraph();
	RebuildDetails();
	ZoomToFitSoon();
}

SProtoBoyDocMapEditor::~SProtoBoyDocMapEditor()
{
	if (FProtoBoyDocMap* Map = GetMap())
	{
		Map->OnChanged.Remove(ChangedHandle);
	}
}

void SProtoBoyDocMapEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	// The graph owns its nodes (UEdGraph::Nodes), so referencing it keeps RootNode and NodesById alive too.
	Collector.AddReferencedObject(Graph);
}

FProtoBoyDocMap* SProtoBoyDocMapEditor::GetMap()
{
	UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	return Subsystem ? &Subsystem->GetDocMap() : nullptr;
}

// --- Layout -------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> SProtoBoyDocMapEditor::MakeSetupRow()
{
	return SNew(SBorder)
		.Visibility_Lambda([]()
		{
			const FProtoBoyDocMap* Map = GetMap();
			return Map && !Map->HasGameDoc() ? EVisibility::Visible : EVisibility::Collapsed;
		})
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
		.Padding(10.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("Icons.WarningWithColor"))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NeedGame", "The map starts with the Game doc: why the game exists, its design pillars, what it is, and an index of every other doc. Set it up from a template, create it on its own, or put an existing note on the map as the Game doc with Map Existing Doc."))
				.AutoWrapText(true)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
				.Text(LOCTEXT("SetupFromTemplate", "Set Up from Template..."))
				.ToolTipText(LOCTEXT("SetupFromTemplateTip", "Pick a preset for your kind of game: the Game doc with suggested design pillars, and a starting set of docs below it."))
				.OnClicked(this, &SProtoBoyDocMapEditor::OnSetupFromTemplateClicked)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(6.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("CreateGame", "Create Game Doc"))
				.ToolTipText(LOCTEXT("CreateGameTip", "Just the Game doc, from its template. Add the rest yourself."))
				.OnClicked(this, &SProtoBoyDocMapEditor::OnCreateGameClicked)
			]
		];
}

TSharedRef<SWidget> SProtoBoyDocMapEditor::MakeToolbar()
{
	return SNew(SWrapBox)
		.UseAllottedSize(true)
		.InnerSlotPadding(FVector2D(6.f, 4.f))

		+ SWrapBox::Slot()
		[
			SNew(SButton)
			.Text(LOCTEXT("SetupFromTemplateToolbar", "Set Up from Template..."))
			.ToolTipText(LOCTEXT("SetupFromTemplateToolbarTip", "Add a preset set of docs for your kind of game. Only docs that don't exist yet are created; nothing is overwritten."))
			.OnClicked(this, &SProtoBoyDocMapEditor::OnSetupFromTemplateClicked)
		]

		+ SWrapBox::Slot()
		[
			SNew(SBox)
			.WidthOverride(200.f)
			[
				SAssignNew(TitleBox, SEditableTextBox)
				.HintText(LOCTEXT("TitleHint", "New doc title"))
				.ToolTipText(LOCTEXT("TitleTip", "Title for the next doc you add. It becomes the file name too."))
			]
		]

		+ SWrapBox::Slot()
		[
			SNew(SComboButton)
			.ToolTipText(LOCTEXT("AddTip", "Add a doc from its template, under the selected doc (or the Game doc)."))
			.IsEnabled_Lambda([]() { const FProtoBoyDocMap* Map = GetMap(); return Map && Map->HasGameDoc(); })
			.OnGetMenuContent(this, &SProtoBoyDocMapEditor::MakeAddMenu)
			.ButtonContent()
			[
				SNew(STextBlock).Text(LOCTEXT("AddDoc", "Add Doc"))
			]
		]

		+ SWrapBox::Slot()
		[
			SNew(SButton)
			.Text(LOCTEXT("AddTech", "Add Tech Doc"))
			.ToolTipText(LOCTEXT("AddTechTip", "Create the selected Design doc's Tech doc next to it, with one section for each of its sections."))
			.IsEnabled_Lambda([this]() { return CanAddTechDoc(SelectedId); })
			.OnClicked_Lambda([this]() { AddTechDoc(SelectedId); return FReply::Handled(); })
		]

		+ SWrapBox::Slot()
		[
			SNew(SComboButton)
			.ToolTipText(LOCTEXT("MapExistingTip", "Put a note that's already in the docs folder on the map, by giving it a layer. Its text isn't changed."))
			.OnGetMenuContent(this, &SProtoBoyDocMapEditor::MakeMapExistingMenu)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text_Lambda([]()
				{
					const FProtoBoyDocMap* Map = GetMap();
					const int32 Count = Map ? Map->GetUnmappedDocs().Num() : 0;
					return Count > 0 ? FText::Format(LOCTEXT("MapExistingCount", "Map Existing Doc ({0})"), FText::AsNumber(Count)) : LOCTEXT("MapExisting", "Map Existing Doc");
				})
			]
		]

		+ SWrapBox::Slot()
		[
			SNew(SButton)
			.Text(LOCTEXT("Open", "Open"))
			.ToolTipText(LOCTEXT("OpenTip", "Open the selected doc (in Obsidian when it's connected)."))
			.IsEnabled_Lambda([this]() { const FProtoBoyDocMap* Map = GetMap(); return Map && Map->Find(SelectedId); })
			.OnClicked_Lambda([this]() { OpenDoc(SelectedId); return FReply::Handled(); })
		]

		+ SWrapBox::Slot()
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("RefreshTip", "Re-read the docs folder (after editing docs outside the editor) and update the Game doc's Areas list."))
			.OnClicked(this, &SProtoBoyDocMapEditor::OnRefreshClicked)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 4.f, 0.f)
				[
					SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.Refresh"))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("Refresh", "Refresh"))
				]
			]
		]

		+ SWrapBox::Slot()
		[
			SNew(SBox)
			.WidthOverride(180.f)
			[
				SNew(SSearchBox)
				.HintText(LOCTEXT("SearchHint", "Search docs"))
				.OnTextChanged_Lambda([this](const FText& Text) { FilterText = Text; })
			]
		];
}

TSharedRef<SWidget> SProtoBoyDocMapEditor::MakeAddMenu()
{
	const FProtoBoyDocMap* Map = GetMap();
	const FString ParentId = GetAddParentId();
	const FProtoBoyDocNode* Parent = Map ? Map->Find(ParentId) : nullptr;

	FMenuBuilder Menu(true, nullptr);
	Menu.BeginSection("Add", Parent
		? FText::Format(LOCTEXT("AddUnder", "Add under {0}"), FText::FromString(Parent->Title))
		: LOCTEXT("AddSection", "Add"));
	for (const EProtoBoyDocKind Kind : ProtoBoyDocLayers::GetAddableKinds())
	{
		Menu.AddMenuEntry(
			ProtoBoyDocMapEditor::GetAddLabel(Kind),
			GetNewTitle().IsEmpty() ? LOCTEXT("NeedTitle", "Type a title in the box first.") : ProtoBoyDocLayers::GetKindDescription(Kind),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Documentation"),
			FUIAction(
				FExecuteAction::CreateSPLambda(this, [this, Kind, ParentId]() { AddDoc(Kind, ParentId); }),
				FCanExecuteAction::CreateSPLambda(this, [this]() { return !GetNewTitle().IsEmpty(); })));
	}
	Menu.EndSection();
	return Menu.MakeWidget();
}

TSharedRef<SWidget> SProtoBoyDocMapEditor::MakeMapExistingMenu()
{
	const FProtoBoyDocMap* Map = GetMap();
	FMenuBuilder Menu(true, nullptr);
	if (!Map || Map->GetUnmappedDocs().IsEmpty())
	{
		Menu.AddMenuEntry(LOCTEXT("NoneUnmapped", "Every doc in the docs folder is on the map."), FText::GetEmpty(), FSlateIcon(),
			FUIAction(FExecuteAction(), FCanExecuteAction::CreateLambda([]() { return false; })));
		return Menu.MakeWidget();
	}

	Menu.BeginSection("Unmapped", LOCTEXT("UnmappedSection", "Docs not on the map"));
	const TArray<FProtoBoyDocInfo>& Unmapped = Map->GetUnmappedDocs();
	for (int32 Index = 0; Index < Unmapped.Num() && Index < ProtoBoyDocMapEditor::MaxUnmappedInMenu; ++Index)
	{
		const FString Path = Unmapped[Index].Path;
		Menu.AddSubMenu(
			FText::FromString(Path),
			LOCTEXT("PickLayerTip", "Choose the layer this doc belongs to."),
			FNewMenuDelegate::CreateSPLambda(this, [this, Path](FMenuBuilder& SubMenu)
			{
				const FProtoBoyDocMap* SubMap = GetMap();
				for (const EProtoBoyDocLayer Layer : ProtoBoyDocLayers::GetAllLayers())
				{
					// There's one Game doc. A mapped Tech doc is shared tech until it names a Design doc to implement.
					if (Layer == EProtoBoyDocLayer::Game && SubMap && SubMap->HasGameDoc())
					{
						continue;
					}
					SubMenu.AddMenuEntry(
						ProtoBoyDocLayers::GetDisplayName(Layer),
						ProtoBoyDocLayers::GetDescription(Layer),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateSPLambda(this, [this, Path, Layer]() { MapExistingDoc(Path, Layer); })));
				}
			}));
	}
	if (Unmapped.Num() > ProtoBoyDocMapEditor::MaxUnmappedInMenu)
	{
		Menu.AddMenuEntry(
			FText::Format(LOCTEXT("MoreUnmapped", "...and {0} more. Add \"protoboy-layer\" to a note's properties to map it by hand."),
				FText::AsNumber(Unmapped.Num() - ProtoBoyDocMapEditor::MaxUnmappedInMenu)),
			FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction(), FCanExecuteAction::CreateLambda([]() { return false; })));
	}
	Menu.EndSection();
	return Menu.MakeWidget();
}

TSharedRef<SWidget> SProtoBoyDocMapEditor::MakeGraph()
{
	Graph = NewObject<UProtoBoyDocMapGraph>(GetTransientPackage(), NAME_None, RF_Transient);
	Graph->Schema = UProtoBoyDocMapGraphSchema::StaticClass();
	Graph->Editor = SharedThis(this);

	GraphCommands = MakeShared<FUICommandList>();

	SGraphEditor::FGraphEditorEvents Events;
	Events.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &SProtoBoyDocMapEditor::OnGraphSelectionChanged);
	Events.OnNodeDoubleClicked = FSingleNodeEvent::CreateSP(this, &SProtoBoyDocMapEditor::OnNodeDoubleClicked);
	Events.OnCreateActionMenuAtLocation = SGraphEditor::FOnCreateActionMenuAtLocation::CreateSP(this, &SProtoBoyDocMapEditor::OnCreateActionMenu);
	Events.OnCreateNodeOrPinMenu = SGraphEditor::FOnCreateNodeOrPinMenu::CreateSP(this, &SProtoBoyDocMapEditor::OnCreateNodeOrPinMenu);

	FGraphAppearanceInfo Appearance;
	Appearance.CornerText = LOCTEXT("CornerText", "DOCS");

	return SNew(SBox)
		.HeightOverride(ProtoBoyDocMapEditor::GraphHeight)
		[
			SAssignNew(GraphEditor, SGraphEditor)
			.GraphToEdit(Graph)
			.AdditionalCommands(GraphCommands)
			.Appearance(Appearance)
			.GraphEvents(Events)
			.ShowGraphStateOverlay(false)
		];
}

// --- Graph events -------------------------------------------------------------------------------------------------

void SProtoBoyDocMapEditor::OnGraphSelectionChanged(const TSet<UObject*>& Selection)
{
	if (bRebuilding)
	{
		return;
	}
	FString NewSelection;
	for (UObject* Object : Selection)
	{
		if (const UProtoBoyDocMapGraphNode* Node = Cast<UProtoBoyDocMapGraphNode>(Object))
		{
			NewSelection = Node->DocId;
			break;
		}
	}
	if (NewSelection != SelectedId)
	{
		SelectedId = NewSelection;
		RebuildDetails();
	}
}

void SProtoBoyDocMapEditor::OnNodeDoubleClicked(UEdGraphNode* Node)
{
	if (const UProtoBoyDocMapGraphNode* DocNode = Cast<UProtoBoyDocMapGraphNode>(Node))
	{
		OpenDoc(DocNode->DocId);
	}
}

FActionMenuContent SProtoBoyDocMapEditor::OnCreateActionMenu(UEdGraph* InGraph, const FVector2f& Location, const TArray<UEdGraphPin*>& DraggedPins,
	bool bAutoExpand, SGraphEditor::FActionMenuClosed OnClosed)
{
	// Dragged off a doc's bottom bar: add under that doc. Right-click in empty space: add under the selection.
	FString ParentId = GetAddParentId();
	for (const UEdGraphPin* Pin : DraggedPins)
	{
		const UProtoBoyDocMapGraphNode* Node = Pin ? Cast<UProtoBoyDocMapGraphNode>(Pin->GetOwningNode()) : nullptr;
		if (Node && Pin->Direction == EGPD_Output)
		{
			ParentId = Node->DocId;
			break;
		}
	}

	const FProtoBoyDocMap* Map = GetMap();
	const FProtoBoyDocNode* Parent = Map ? Map->Find(ParentId) : nullptr;
	FMenuBuilder Menu(true, nullptr);
	Menu.BeginSection("Add", Parent ? FText::Format(LOCTEXT("ActionAddUnder", "Add under {0}"), FText::FromString(Parent->Title)) : LOCTEXT("ActionAdd", "Add"));
	if (!Map || !Map->HasGameDoc())
	{
		Menu.AddMenuEntry(LOCTEXT("ActionNeedGame", "Create the Game doc first."), FText::GetEmpty(), FSlateIcon(),
			FUIAction(FExecuteAction(), FCanExecuteAction::CreateLambda([]() { return false; })));
	}
	else
	{
		const FString Title = GetNewTitle();
		for (const EProtoBoyDocKind Kind : ProtoBoyDocLayers::GetAddableKinds())
		{
			Menu.AddMenuEntry(
				Title.IsEmpty()
					? ProtoBoyDocMapEditor::GetAddLabel(Kind)
					: FText::Format(LOCTEXT("ActionAddKindTitled", "{0} \"{1}\""), ProtoBoyDocMapEditor::GetAddLabel(Kind), FText::FromString(Title)),
				Title.IsEmpty() ? LOCTEXT("ActionNeedTitle", "Type a title in the toolbar's title box first.") : ProtoBoyDocLayers::GetKindDescription(Kind),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Documentation"),
				FUIAction(
					FExecuteAction::CreateSPLambda(this, [this, Kind, ParentId, OnClosed]()
					{
						OnClosed.ExecuteIfBound();
						AddDoc(Kind, ParentId);
					}),
					FCanExecuteAction::CreateLambda([Title]() { return !Title.IsEmpty(); })));
		}
	}
	Menu.EndSection();

	FActionMenuContent Content(Menu.MakeWidget());
	Content.OnMenuDismissed.AddLambda([OnClosed]() { OnClosed.ExecuteIfBound(); });
	return Content;
}

FActionMenuContent SProtoBoyDocMapEditor::OnCreateNodeOrPinMenu(UEdGraph* InGraph, const UEdGraphNode* Node, const UEdGraphPin* Pin, FMenuBuilder* Menu, bool bReadOnly)
{
	const UProtoBoyDocMapGraphNode* DocNode = Cast<UProtoBoyDocMapGraphNode>(Node);
	const FProtoBoyDocMap* Map = GetMap();
	const FProtoBoyDocNode* Doc = DocNode && Map ? Map->Find(DocNode->DocId) : nullptr;
	if (!Doc || !Menu)
	{
		return FActionMenuContent();
	}
	const FString Id = Doc->Id;
	if (SelectedId != Id)
	{
		SelectedId = Id;
		RebuildDetails();
	}

	Menu->BeginSection("Doc", FText::FromString(Doc->Title));
	Menu->AddMenuEntry(
		LOCTEXT("CtxOpen", "Open"),
		FText::FromString(Doc->Path),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.OpenInExternalEditor"),
		FUIAction(FExecuteAction::CreateSPLambda(this, [this, Id]() { OpenDoc(Id); })));
	if (CanAddTechDoc(Id))
	{
		Menu->AddMenuEntry(
			LOCTEXT("CtxAddTech", "Add Tech Doc"),
			LOCTEXT("CtxAddTechTip", "Create this Design doc's Tech doc next to it."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"),
			FUIAction(FExecuteAction::CreateSPLambda(this, [this, Id]() { AddTechDoc(Id); })));
	}
	const FProtoBoyDocNode* Parent = Map->GetParent(Id);
	const FProtoBoyDocNode* Default = Map->GetDefaultParent(*Doc);
	if (Parent && Default && Parent != Default)
	{
		Menu->AddMenuEntry(
			FText::Format(LOCTEXT("CtxDefault", "Move Back Under {0}"), FText::FromString(Default->Title)),
			LOCTEXT("CtxDefaultTip", "Where docs of this layer go by default."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSPLambda(this, [this, Id]() { ReparentFromGraph(Id, FString()); })));
	}
	Menu->EndSection();

	if (Map->HasGameDoc())
	{
		Menu->BeginSection("AddUnder", LOCTEXT("CtxAddUnder", "Add Under This Doc"));
		const FString Title = GetNewTitle();
		for (const EProtoBoyDocKind Kind : ProtoBoyDocLayers::GetAddableKinds())
		{
			Menu->AddMenuEntry(
				ProtoBoyDocMapEditor::GetAddLabel(Kind),
				Title.IsEmpty() ? LOCTEXT("CtxNeedTitle", "Type a title in the toolbar's title box first.") : ProtoBoyDocLayers::GetKindDescription(Kind),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSPLambda(this, [this, Kind, Id]() { AddDoc(Kind, Id); }),
					FCanExecuteAction::CreateLambda([Title]() { return !Title.IsEmpty(); })));
		}
		Menu->EndSection();
	}
	return FActionMenuContent(Menu->MakeWidget());
}

void SProtoBoyDocMapEditor::ReparentFromGraph(const FString& Id, const FString& NewParentId)
{
	if (FProtoBoyDocMap* Map = GetMap())
	{
		SelectedId = Id;
		FText Error;
		if (!Map->SetParent(Id, NewParentId, Error))
		{
			SetStatus(Error, true);
			ScheduleRebuild();
			return;
		}
		SetStatus(FText::GetEmpty(), false);
	}
}

bool SProtoBoyDocMapEditor::MatchesFilter(const FString& Id) const
{
	const FString Filter = FilterText.ToString().TrimStartAndEnd();
	if (Filter.IsEmpty())
	{
		return true;
	}
	const FProtoBoyDocMap* Map = GetMap();
	const FProtoBoyDocNode* Doc = Map ? Map->Find(Id) : nullptr;
	return Doc && (Doc->Title.Contains(Filter) || Doc->Path.Contains(Filter) || Doc->Summary.Contains(Filter));
}

// --- Rebuild ------------------------------------------------------------------------------------------------------

void SProtoBoyDocMapEditor::ScheduleRebuild()
{
	// Deferred: changes arrive from inside graph callbacks (wire drops, menus), and rebuilding the graph there would
	// destroy the widget still running the callback.
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

void SProtoBoyDocMapEditor::RebuildGraph()
{
	const FProtoBoyDocMap* Map = GetMap();
	if (!Graph || !Map)
	{
		return;
	}

	// Only docs that hang below the Game doc are drawn; without one there's no map yet.
	const FProtoBoyDocNode* Root = Map->GetGame();
	TSet<FString> Live;
	if (Root)
	{
		for (const FProtoBoyDocNode& Doc : Map->GetNodes())
		{
			if (Map->IsInSubtree(Doc.Id, Root->Id))
			{
				Live.Add(Doc.Id);
			}
		}
	}

	// Recreate nodes whose root-ness changed (pins differ), drop the gone, add the new.
	for (auto It = NodesById.CreateIterator(); It; ++It)
	{
		const bool bShouldBeRoot = Root && It.Key() == Root->Id;
		if (!Live.Contains(It.Key()) || It.Value()->IsRoot() != bShouldBeRoot)
		{
			Graph->RemoveNode(It.Value());
			It.RemoveCurrent();
		}
	}
	for (const FString& Id : Live)
	{
		if (!NodesById.Contains(Id))
		{
			UProtoBoyDocMapGraphNode* Node = NewObject<UProtoBoyDocMapGraphNode>(Graph, NAME_None, RF_Transient);
			Node->DocId = Id;
			Node->bRootNode = Id == Root->Id;
			Node->bCanRenameNode = false;
			Node->CreateNewGuid();
			Node->AllocateDefaultPins();
			Graph->AddNode(Node, false, false);
			NodesById.Add(Id, Node);
		}
	}
	RootNode = Root ? NodesById.FindRef(Root->Id) : nullptr;
	Graph->LinkAndLayout(RootNode, NodesById);

	if (!Map->Find(SelectedId))
	{
		SelectedId.Reset();
	}
	Graph->NotifyGraphChanged();

	if (GraphEditor.IsValid())
	{
		bRebuilding = true;
		GraphEditor->ClearSelectionSet();
		if (const TObjectPtr<UProtoBoyDocMapGraphNode>* Selected = NodesById.Find(SelectedId))
		{
			GraphEditor->SetNodeSelection(*Selected, true);
		}
		bRebuilding = false;
	}
}

void SProtoBoyDocMapEditor::ZoomToFitSoon()
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

void SProtoBoyDocMapEditor::Select(const FString& Id)
{
	SelectedId = Id;
	ScheduleRebuild();
	if (const TObjectPtr<UProtoBoyDocMapGraphNode>* Node = NodesById.Find(Id); Node && GraphEditor.IsValid())
	{
		GraphEditor->JumpToNode(*Node, false, true);
	}
}

void SProtoBoyDocMapEditor::SetStatus(const FText& Text, bool bError)
{
	StatusText = Text;
	bStatusIsError = bError;
}

// --- Details ------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> SProtoBoyDocMapEditor::MakeLabel(const FText& Text)
{
	return SNew(SBox)
		.WidthOverride(110.f)
		[
			SNew(STextBlock).Text(Text).ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
}

TSharedRef<SWidget> SProtoBoyDocMapEditor::MakeDocLink(const FProtoBoyDocNode& Doc)
{
	const FString Id = Doc.Id;
	return SNew(SHyperlink)
		.Text(FText::FromString(Doc.Title))
		.ToolTipText(FText::FromString(Doc.Path))
		.OnNavigate_Lambda([this, Id]() { Select(Id); });
}

void SProtoBoyDocMapEditor::RebuildDetails()
{
	if (!DetailsBox.IsValid())
	{
		return;
	}
	const FProtoBoyDocMap* Map = GetMap();
	const FProtoBoyDocNode* Doc = Map ? Map->Find(SelectedId) : nullptr;
	if (!Doc)
	{
		DetailsBox->SetContent(
			SNew(STextBlock)
			.Visibility_Lambda([]() { const FProtoBoyDocMap* M = GetMap(); return M && M->HasGameDoc() ? EVisibility::Visible : EVisibility::Collapsed; })
			.Text(LOCTEXT("SelectDoc", "Select a doc to see where it sits and what's linked to it."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground()));
		return;
	}
	DetailsBox->SetContent(MakeDetails(*Doc));
}

TSharedRef<SWidget> SProtoBoyDocMapEditor::MakeDetails(const FProtoBoyDocNode& Doc)
{
	const FProtoBoyDocMap* Map = GetMap();
	const FString Id = Doc.Id;

	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	auto AddRow = [&Rows, this](const FText& Label, const TSharedRef<SWidget>& Value)
	{
		Rows->AddSlot()
		.AutoHeight()
		.Padding(0.f, 3.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)[ MakeLabel(Label) ]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Top)[ Value ]
		];
	};
	auto Text = [](const FText& Value, bool bSubdued = false)
	{
		return SNew(STextBlock)
			.Text(Value)
			.ColorAndOpacity(bSubdued ? FSlateColor::UseSubduedForeground() : FSlateColor::UseForeground())
			.AutoWrapText(true);
	};

	// Title
	Rows->AddSlot()
	.AutoHeight()
	.Padding(0.f, 0.f, 0.f, 4.f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(Doc.Title))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
	];

	AddRow(LOCTEXT("LayerLabel", "Layer"), Text(FText::Format(LOCTEXT("LayerValue", "{0}: {1}"),
		ProtoBoyDocLayers::GetDisplayName(Doc.Layer), ProtoBoyDocLayers::GetDescription(Doc.Layer))));

	AddRow(LOCTEXT("PathLabel", "File"),
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[ Text(FText::FromString(Doc.Path)) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f, 0.f, 0.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("OpenDoc", "Open"))
			.OnClicked_Lambda([this, Id]() { OpenDoc(Id); return FReply::Handled(); })
		]);

	if (const FProtoBoyDocNode* Parent = Map->GetParent(Id))
	{
		const bool bDefault = Parent == Map->GetDefaultParent(Doc);
		AddRow(LOCTEXT("UnderLabel", "Under"),
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ MakeDocLink(*Parent) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f, 0.f, 0.f)
			[
				Text(bDefault ? LOCTEXT("DefaultPlace", "(where this layer goes by default)") : LOCTEXT("MovedPlace", "(set in this doc's frontmatter)"), true)
			]);
	}

	if (Doc.Layer == EProtoBoyDocLayer::Design)
	{
		if (const FProtoBoyDocNode* Tech = Map->FindTechFor(Id))
		{
			AddRow(LOCTEXT("TechLabel", "Tech doc"), MakeDocLink(*Tech));
		}
		else
		{
			AddRow(LOCTEXT("TechLabel", "Tech doc"),
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Text(LOCTEXT("NoTech", "None yet: how this is built isn't written down."), true) ]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("AddTechDetails", "Add Tech Doc"))
					.OnClicked_Lambda([this, Id]() { AddTechDoc(Id); return FReply::Handled(); })
				]);
		}
	}
	if (Doc.Layer == EProtoBoyDocLayer::Tech)
	{
		if (const FProtoBoyDocNode* Design = Map->Find(Doc.ImplementsId))
		{
			AddRow(LOCTEXT("ImplementsLabel", "Implements"), MakeDocLink(*Design));
		}
		else
		{
			AddRow(LOCTEXT("ImplementsLabel", "Implements"), Text(Doc.ImplementsId.IsEmpty()
				? LOCTEXT("ImplementsNone", "Nothing: this is shared tech. To make it a feature's Tech doc, add \"protoboy-implements: <design doc id>\" to its frontmatter.")
				: FText::Format(LOCTEXT("ImplementsMissing", "\"{0}\", which isn't on the map."), FText::FromString(Doc.ImplementsId)), true));
		}
	}
	if (&Doc == Map->GetGame())
	{
		const FProtoBoyGameDoc Game = Map->ReadGame();
		TArray<FString> Names;
		for (const FProtoBoyDesignPillar& Pillar : Game.Pillars)
		{
			Names.Add(Pillar.Name);
		}
		AddRow(LOCTEXT("PillarsLabel", "Design pillars"), Text(Names.IsEmpty()
			? LOCTEXT("NoPillars", "None written yet: add one ### heading per pillar under Design Pillars.")
			: FText::FromString(FString::Join(Names, TEXT(", "))), Names.IsEmpty()));
	}

	const TArray<const FProtoBoyDocNode*> Children = Map->GetChildren(Id);
	if (!Children.IsEmpty())
	{
		TSharedRef<SWrapBox> Links = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(10.f, 2.f));
		for (const FProtoBoyDocNode* Child : Children)
		{
			Links->AddSlot()[ MakeDocLink(*Child) ];
		}
		AddRow(LOCTEXT("BelowLabel", "Below it"), Links);
	}

	AddRow(LOCTEXT("SummaryLabel", "Starts with"), Text(Doc.Summary.IsEmpty()
		? LOCTEXT("NoSummary", "Nothing written yet below the template's headings.")
		: FText::FromString(Doc.Summary), Doc.Summary.IsEmpty()));

	return SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
		.Padding(10.f)
		[
			Rows
		];
}

// --- Actions ------------------------------------------------------------------------------------------------------

FString SProtoBoyDocMapEditor::GetAddParentId() const
{
	const FProtoBoyDocMap* Map = GetMap();
	if (!Map)
	{
		return FString();
	}
	if (Map->Find(SelectedId))
	{
		return SelectedId;
	}
	const FProtoBoyDocNode* Root = Map->GetGame();
	return Root ? Root->Id : FString();
}

FString SProtoBoyDocMapEditor::GetNewTitle() const
{
	return TitleBox.IsValid() ? TitleBox->GetText().ToString().TrimStartAndEnd() : FString();
}

bool SProtoBoyDocMapEditor::CanAddTechDoc(const FString& Id) const
{
	const FProtoBoyDocMap* Map = GetMap();
	const FProtoBoyDocNode* Doc = Map ? Map->Find(Id) : nullptr;
	return Doc && Doc->Layer == EProtoBoyDocLayer::Design && !Map->FindTechFor(Id);
}

void SProtoBoyDocMapEditor::AddDoc(EProtoBoyDocKind Kind, const FString& ParentId)
{
	FProtoBoyDocMap* Map = GetMap();
	if (!Map)
	{
		return;
	}
	FString NewId;
	FText Error;
	if (!Map->CreateDoc(Kind, GetNewTitle(), ParentId, NewId, Error))
	{
		SetStatus(Error, true);
		return;
	}
	if (TitleBox.IsValid())
	{
		TitleBox->SetText(FText::GetEmpty());
	}
	const FProtoBoyDocNode* Doc = Map->Find(NewId);
	SetStatus(FText::Format(LOCTEXT("Created", "Created {0}."), FText::FromString(Doc ? Doc->Path : NewId)), false);
	Select(NewId);
}

void SProtoBoyDocMapEditor::AddTechDoc(const FString& DesignId)
{
	FProtoBoyDocMap* Map = GetMap();
	if (!Map)
	{
		return;
	}
	FString NewId;
	FText Error;
	if (!Map->CreateTechDoc(DesignId, NewId, Error))
	{
		SetStatus(Error, true);
		return;
	}
	const FProtoBoyDocNode* Doc = Map->Find(NewId);
	SetStatus(FText::Format(LOCTEXT("CreatedTech", "Created {0}."), FText::FromString(Doc ? Doc->Path : NewId)), false);
	Select(NewId);
}

void SProtoBoyDocMapEditor::MapExistingDoc(const FString& Path, EProtoBoyDocLayer Layer)
{
	FProtoBoyDocMap* Map = GetMap();
	if (!Map)
	{
		return;
	}
	FText Error;
	if (!Map->AddToMap(Path, Layer, Error))
	{
		SetStatus(Error, true);
		return;
	}
	SetStatus(FText::Format(LOCTEXT("Mapped", "{0} is on the map as a {1} doc."), FText::FromString(Path), ProtoBoyDocLayers::GetDisplayName(Layer)), false);
	if (const FProtoBoyDocNode* Doc = Map->FindByPath(Path))
	{
		Select(Doc->Id);
	}
}

void SProtoBoyDocMapEditor::OpenDoc(const FString& Id)
{
	const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	const FProtoBoyDocNode* Doc = Subsystem ? Subsystem->GetDocMap().Find(Id) : nullptr;
	if (!Doc)
	{
		return;
	}
	if (Subsystem->GetObsidianConnector().IsConnected())
	{
		FText Error;
		if (!Subsystem->GetObsidianConnector().OpenInObsidian(Doc->Path, Error))
		{
			SetStatus(Error, true);
		}
		return;
	}
	const FString File = FPaths::ConvertRelativePathToFull(Subsystem->GetDocGenerator().GetDocsRoot() / Doc->Path);
	FPlatformProcess::LaunchFileInDefaultExternalApplication(*File);
}

FReply SProtoBoyDocMapEditor::OnCreateGameClicked()
{
	if (FProtoBoyDocMap* Map = GetMap())
	{
		FText Error;
		if (Map->CreateGameDoc(Error))
		{
			SetStatus(LOCTEXT("CreatedGame", "Created the Game doc. Write the pitch and the design pillars first."), false);
			ZoomToFitSoon();
		}
		else
		{
			SetStatus(Error, true);
		}
	}
	return FReply::Handled();
}

FReply SProtoBoyDocMapEditor::OnSetupFromTemplateClicked()
{
	SetStatus(FText::GetEmpty(), false);
	// Modal: returns once the window closes. The map fires OnChanged if the template wrote anything.
	SProtoBoySetupDocsDialog::Open(AsShared());
	ZoomToFitSoon();
	return FReply::Handled();
}

FReply SProtoBoyDocMapEditor::OnRefreshClicked()
{
	if (FProtoBoyDocMap* Map = GetMap())
	{
		Map->Refresh();
		FText Error;
		if (!Map->UpdateAreasIndex(Error))
		{
			SetStatus(Error, true);
			return FReply::Handled();
		}
		SetStatus(FText::GetEmpty(), false);
		ZoomToFitSoon();
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
