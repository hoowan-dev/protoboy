#include "UI/Docs/Map/SPBDocMapEditor.h"

#include "Docs/Obsidian/PBObsidianConnector.h"
#include "Docs/PBDocGenerator.h"
#include "Docs/PBDocMap.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformProcess.h"
#include "Misc/MessageDialog.h"
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
	/** Longest list of existing notes in the add menu before it says "and N more". */
	constexpr int32 MaxUnmappedInMenu = 40;

	/** "Design Doc", "Tech Doc", "Content Doc", "Style Doc". */
	FText GetAddLabel(EProtoBoyDocKind Kind)
	{
		return FText::Format(NSLOCTEXT("SProtoBoyDocMapEditor", "AddKind", "{0} Doc"), ProtoBoyDocLayers::GetKindName(Kind));
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
			.Text(LOCTEXT("Intro", "The project's docs: the Game doc (what the game is, named after the project), Design docs (what a player should be able to do or see), Tech docs (the technical details of how something works) and Content docs (the assets to make it happen, and the style they must have, which agents read before looking for assets). Put any doc under any other, however makes sense to you; only the Game doc stays at the top. Each doc's place is stored in its own frontmatter, so the docs are the source of truth."))
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

		// An old Vision and Overview to combine
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 8.f, 0.f, 0.f)
		[
			MakeCombineRow()
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
			.Text(LOCTEXT("GraphHelp", "Drag off a doc's bottom bar into empty space to add a doc under it, or to link an existing note. Wire a doc's bottom bar to another doc's top bar to move the second under the first. Delete removes the selected doc (after asking). Double-click opens a doc."))
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
				.Text(FText::Format(LOCTEXT("NeedGame", "The map starts with the Game doc, {0}: why the game exists, its design pillars, what it is, and an index of every other doc. Set it up from a template, or create it on its own."),
					FText::FromString(FProtoBoyDocMap::GetProjectTitle())))
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
				.ToolTipText(LOCTEXT("SetupFromTemplateTip", "Pick a preset for your kind of game, or Blank for just the Game doc."))
				.OnClicked(this, &SProtoBoyDocMapEditor::OnSetupFromTemplateClicked)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(6.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("CreateGame", "Create Game Doc"))
				.ToolTipText(LOCTEXT("CreateGameTip", "Just the Game doc, from its template. Add the rest by dragging off it."))
				.OnClicked(this, &SProtoBoyDocMapEditor::OnCreateGameClicked)
			]
		];
}

TSharedRef<SWidget> SProtoBoyDocMapEditor::MakeCombineRow()
{
	return SNew(SBorder)
		.Visibility_Lambda([]()
		{
			const FProtoBoyDocMap* Map = GetMap();
			return Map && Map->HasSplitGameDocs() ? EVisibility::Visible : EVisibility::Collapsed;
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
				.Image(FAppStyle::Get().GetBrush("Icons.InfoWithColor"))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([]()
				{
					const FProtoBoyDocMap* Map = GetMap();
					TArray<FString> Paths;
					for (const FProtoBoyDocNode* Game : Map ? Map->GetGameDocs() : TArray<const FProtoBoyDocNode*>())
					{
						Paths.Add(Game->Path);
					}
					return FText::Format(LOCTEXT("CombineText", "{0} are all top-level docs from an older ProtoBoy (a separate Vision and Overview). Combine them into one Game doc titled {1}: every section is kept, and the old files go to {2}."),
						FText::FromString(FString::Join(Paths, TEXT(", "))), FText::FromString(FProtoBoyDocMap::GetProjectTitle()),
						UProtoBoySubsystem::Get() ? UProtoBoySubsystem::Get()->GetDocsProvider().GetDeletedDocsLocation() : FText::GetEmpty());
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
				.Text(LOCTEXT("Combine", "Combine into One Game Doc"))
				.OnClicked(this, &SProtoBoyDocMapEditor::OnCombineClicked)
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

TSharedRef<SWidget> SProtoBoyDocMapEditor::MakeGraph()
{
	Graph = NewObject<UProtoBoyDocMapGraph>(GetTransientPackage(), NAME_None, RF_Transient);
	Graph->Schema = UProtoBoyDocMapGraphSchema::StaticClass();
	Graph->Editor = SharedThis(this);

	GraphCommands = MakeShared<FUICommandList>();
	GraphCommands->MapAction(FGenericCommands::Get().Delete,
		FExecuteAction::CreateSPLambda(this, [this]() { DeleteDocWithConfirm(SelectedId); }),
		FCanExecuteAction::CreateSP(this, &SProtoBoyDocMapEditor::CanDeleteSelected));

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

// --- Add menu -----------------------------------------------------------------------------------------------------

TArray<EProtoBoyDocKind> SProtoBoyDocMapEditor::GetKindsUnder(const FProtoBoyDocNode& Parent) const
{
	// The usual kinds under this layer first, then every other addable kind: the map allows any layer under any doc.
	const FProtoBoyDocMap* Map = GetMap();
	TArray<EProtoBoyDocKind> Kinds;
	switch (Parent.Layer)
	{
	case EProtoBoyDocLayer::Game:
		break;
	case EProtoBoyDocLayer::Design:
		Kinds.Append({ EProtoBoyDocKind::Tech, EProtoBoyDocKind::Design, EProtoBoyDocKind::Content });
		break;
	case EProtoBoyDocLayer::Tech:
		Kinds.Append({ EProtoBoyDocKind::Tech, EProtoBoyDocKind::Content });
		break;
	case EProtoBoyDocLayer::Content:
		Kinds.Add(EProtoBoyDocKind::Content);
		break;
	}
	for (const EProtoBoyDocKind Kind : ProtoBoyDocLayers::GetAddableKinds())
	{
		Kinds.AddUnique(Kind);
	}
	return Kinds;
}

TSharedRef<SWidget> SProtoBoyDocMapEditor::MakeAddMenu(const FString& ParentId, TSharedPtr<SWidget>& OutFocus, TFunction<void()> OnPicked)
{
	const FProtoBoyDocMap* Map = GetMap();
	const FProtoBoyDocNode* Parent = Map ? Map->Find(ParentId) : nullptr;
	FMenuBuilder Menu(true, nullptr);
	if (!Parent)
	{
		Menu.AddMenuEntry(LOCTEXT("AddNeedGame", "Create the Game doc first."), FText::GetEmpty(), FSlateIcon(),
			FUIAction(FExecuteAction(), FCanExecuteAction::CreateLambda([]() { return false; })));
		return Menu.MakeWidget();
	}

	const TArray<EProtoBoyDocKind> Kinds = GetKindsUnder(*Parent);
	const TSharedRef<FString> Title = MakeShared<FString>();

	// New docs: a title first, then what fits under this doc.
	Menu.BeginSection("New", FText::Format(LOCTEXT("NewUnder", "New doc under {0}"), FText::FromString(Parent->Title)));
	TSharedRef<SEditableTextBox> TitleBox = SNew(SEditableTextBox)
		.HintText(LOCTEXT("TitleHint", "Title (becomes the file name)"))
		.OnTextChanged_Lambda([Title](const FText& Text) { *Title = Text.ToString().TrimStartAndEnd(); })
		.OnTextCommitted_Lambda([this, Title, Kinds, ParentId, OnPicked](const FText&, ETextCommit::Type Commit)
		{
			// Enter creates the first kind that takes a title.
			if (Commit == ETextCommit::OnEnter && !Kinds.IsEmpty() && !Title->IsEmpty())
			{
				FSlateApplication::Get().DismissAllMenus();
				OnPicked();
				AddDoc(Kinds[0], ParentId, *Title);
			}
		});
	OutFocus = TitleBox;
	Menu.AddWidget(SNew(SBox).WidthOverride(260.f).Padding(FMargin(8.f, 2.f))[ TitleBox ], FText::GetEmpty(), true);
	for (const EProtoBoyDocKind Kind : Kinds)
	{
		Menu.AddMenuEntry(
			ProtoBoyDocMapEditor::GetAddLabel(Kind),
			ProtoBoyDocLayers::GetKindDescription(Kind),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Documentation"),
			FUIAction(
				FExecuteAction::CreateSPLambda(this, [this, Kind, ParentId, Title, OnPicked]()
				{
					OnPicked();
					AddDoc(Kind, ParentId, *Title);
				}),
				FCanExecuteAction::CreateLambda([Title]() { return !Title->IsEmpty(); })));
	}
	Menu.EndSection();

	// Existing notes in the docs folder that aren't on the map yet, linked here as one of the same kinds.
	const TArray<FProtoBoyDocInfo>& Unmapped = Map->GetUnmappedDocs();
	if (!Unmapped.IsEmpty())
	{
		Menu.BeginSection("Existing", LOCTEXT("ExistingSection", "Existing note"));
		Menu.AddSubMenu(
			FText::Format(LOCTEXT("LinkExisting", "Link an Existing Note ({0})"), FText::AsNumber(Unmapped.Num())),
			LOCTEXT("LinkExistingTip", "Put a note that's already in the docs folder under this doc. Only its frontmatter changes."),
			FNewMenuDelegate::CreateSPLambda(this, [this, ParentId, Kinds, OnPicked](FMenuBuilder& Notes)
			{
				const FProtoBoyDocMap* NotesMap = GetMap();
				const FProtoBoyDocNode* NotesParent = NotesMap ? NotesMap->Find(ParentId) : nullptr;
				if (!NotesParent)
				{
					return;
				}
				const TArray<FProtoBoyDocInfo>& List = NotesMap->GetUnmappedDocs();
				for (int32 Index = 0; Index < List.Num() && Index < ProtoBoyDocMapEditor::MaxUnmappedInMenu; ++Index)
				{
					const FString Path = List[Index].Path;
					Notes.AddSubMenu(
						FText::FromString(Path),
						LOCTEXT("LinkAsTip", "Link it as..."),
						FNewMenuDelegate::CreateSPLambda(this, [this, Path, ParentId, Kinds, OnPicked](FMenuBuilder& As)
						{
							const FProtoBoyDocMap* AsMap = GetMap();
							if (!AsMap || !AsMap->Find(ParentId))
							{
								return;
							}
							for (const EProtoBoyDocKind Kind : Kinds)
							{
								const EProtoBoyDocLayer Layer = ProtoBoyDocLayers::GetLayer(Kind);
								As.AddMenuEntry(
									ProtoBoyDocMapEditor::GetAddLabel(Kind),
									ProtoBoyDocLayers::GetKindDescription(Kind),
									FSlateIcon(),
									FUIAction(FExecuteAction::CreateSPLambda(this, [this, Path, Layer, ParentId, OnPicked]()
									{
										OnPicked();
										LinkExistingDoc(Path, Layer, ParentId);
									})));
							}
						}));
				}
				if (List.Num() > ProtoBoyDocMapEditor::MaxUnmappedInMenu)
				{
					Notes.AddMenuEntry(
						FText::Format(LOCTEXT("MoreUnmapped", "...and {0} more. Add \"protoboy-layer\" to a note's properties to map it by hand."),
							FText::AsNumber(List.Num() - ProtoBoyDocMapEditor::MaxUnmappedInMenu)),
						FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction(), FCanExecuteAction::CreateLambda([]() { return false; })));
				}
			}));
		Menu.EndSection();
	}
	return Menu.MakeWidget();
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
	// Dragged off a doc's bottom bar: add under that doc. Right-click in empty space: add under the Game doc.
	const FProtoBoyDocMap* Map = GetMap();
	const FProtoBoyDocNode* Root = Map ? Map->GetGame() : nullptr;
	FString ParentId = Root ? Root->Id : FString();
	for (const UEdGraphPin* Pin : DraggedPins)
	{
		const UProtoBoyDocMapGraphNode* Node = Pin ? Cast<UProtoBoyDocMapGraphNode>(Pin->GetOwningNode()) : nullptr;
		if (Node && Pin->Direction == EGPD_Output)
		{
			ParentId = Node->DocId;
			break;
		}
	}

	// Built first: it fills Focus (the title box), which the menu focuses so you can type straight away.
	TSharedPtr<SWidget> Focus;
	const TSharedRef<SWidget> MenuWidget = MakeAddMenu(ParentId, Focus, [OnClosed]() { OnClosed.ExecuteIfBound(); });
	FActionMenuContent Content(MenuWidget, Focus);
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
	const FProtoBoyDocNode* Parent = Map->GetParent(Id);
	const FProtoBoyDocNode* Default = Map->GetDefaultParent(*Doc);
	if (Parent && Default && Parent != Default)
	{
		Menu->AddMenuEntry(
			FText::Format(LOCTEXT("CtxDefault", "Move Back Under {0}"), FText::FromString(Default->Title)),
			LOCTEXT("CtxDefaultTip", "Where docs go by default."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSPLambda(this, [this, Id]() { ReparentFromGraph(Id, FString()); })));
	}
	if (Doc != Map->GetGame())
	{
		Menu->AddMenuSeparator();
		Menu->AddMenuEntry(FGenericCommands::Get().Delete, NAME_None, LOCTEXT("CtxDelete", "Delete..."),
			LOCTEXT("CtxDeleteTip", "Delete this doc. Asks first; the note goes to the trash."));
	}
	Menu->EndSection();
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
				Text(bDefault ? LOCTEXT("DefaultPlace", "(where docs go by default)") : LOCTEXT("MovedPlace", "(set in this doc's frontmatter)"), true)
			]);
	}

	if (Doc.Layer == EProtoBoyDocLayer::Content)
	{
		AddRow(LOCTEXT("StyleUseLabel", "Style"), Text(LOCTEXT("StyleUse", "Agents read this doc's Style, What to Look For and What to Avoid sections (and those of the Content docs above it) before searching asset stores (Fab) or choosing assets."), true));
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

void SProtoBoyDocMapEditor::AddDoc(EProtoBoyDocKind Kind, const FString& ParentId, const FString& Title)
{
	FProtoBoyDocMap* Map = GetMap();
	if (!Map)
	{
		return;
	}
	FString NewId;
	FText Error;
	if (!Map->CreateDoc(Kind, Title, ParentId, NewId, Error))
	{
		SetStatus(Error, true);
		return;
	}
	const FProtoBoyDocNode* Doc = Map->Find(NewId);
	SetStatus(FText::Format(LOCTEXT("Created", "Created {0}."), FText::FromString(Doc ? Doc->Path : NewId)), false);
	Select(NewId);
}

void SProtoBoyDocMapEditor::LinkExistingDoc(const FString& Path, EProtoBoyDocLayer Layer, const FString& ParentId)
{
	FProtoBoyDocMap* Map = GetMap();
	if (!Map)
	{
		return;
	}
	FText Error;
	if (!Map->AddToMap(Path, Layer, ParentId, Error))
	{
		SetStatus(Error, true);
		return;
	}
	SetStatus(FText::Format(LOCTEXT("Linked", "{0} is on the map as a {1} doc."), FText::FromString(Path), ProtoBoyDocLayers::GetDisplayName(Layer)), false);
	if (const FProtoBoyDocNode* Doc = Map->FindByPath(Path))
	{
		Select(Doc->Id);
	}
}

bool SProtoBoyDocMapEditor::CanDeleteSelected() const
{
	const FProtoBoyDocMap* Map = GetMap();
	const FProtoBoyDocNode* Doc = Map ? Map->Find(SelectedId) : nullptr;
	return Doc && Doc != Map->GetGame();
}

void SProtoBoyDocMapEditor::DeleteDocWithConfirm(const FString& Id)
{
	UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	FProtoBoyDocMap* Map = GetMap();
	const FProtoBoyDocNode* Doc = Map ? Map->Find(Id) : nullptr;
	if (!Subsystem || !Doc || Doc == Map->GetGame())
	{
		return;
	}

	// Say exactly what happens: the note itself goes, and what's under it moves up.
	FText Consequences;
	const int32 NumChildren = Map->GetChildren(Id).Num();
	if (NumChildren > 0)
	{
		Consequences = FText::Format(LOCTEXT("DeleteChildren", "\n\nThe {0} {0}|plural(one=doc,other=docs) under it {0}|plural(one=isn't,other=aren't) deleted: {0}|plural(one=it moves,other=they move) back under the Game doc."), FText::AsNumber(NumChildren));
	}
	const FText Message = FText::Format(LOCTEXT("DeleteConfirm", "Delete \"{0}\"?\n\nThis deletes the note itself ({1}): it's moved to {2}.{3}"),
		FText::FromString(Doc->Title), FText::FromString(Doc->Path), Subsystem->GetDocsProvider().GetDeletedDocsLocation(), Consequences);
	if (FMessageDialog::Open(EAppMsgType::YesNo, EAppReturnType::No, Message, LOCTEXT("DeleteTitle", "Delete Doc")) != EAppReturnType::Yes)
	{
		return;
	}

	const FString Path = Doc->Path;
	FText Error;
	if (!Map->DeleteDoc(Id, Error))
	{
		SetStatus(Error, true);
		return;
	}
	SelectedId.Reset();
	SetStatus(FText::Format(LOCTEXT("Deleted", "Deleted {0}."), FText::FromString(Path)), false);
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
			SetStatus(LOCTEXT("CreatedGame", "Created the Game doc. Write the pitch and the design pillars first, then drag off it to add docs."), false);
			ZoomToFitSoon();
		}
		else
		{
			SetStatus(Error, true);
		}
	}
	return FReply::Handled();
}

FReply SProtoBoyDocMapEditor::OnCombineClicked()
{
	if (FProtoBoyDocMap* Map = GetMap())
	{
		FText Error;
		if (Map->CombineGameDocs(Error))
		{
			SetStatus(FText::Format(LOCTEXT("Combined", "Combined into {0}, titled {1}."), FText::FromString(FProtoBoyDocMap::GetGameDocPath()), FText::FromString(FProtoBoyDocMap::GetProjectTitle())), false);
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
