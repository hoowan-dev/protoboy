#pragma once

#include "Docs/PBDocLayers.h"
#include "GraphEditor.h"
#include "UObject/GCObject.h"
#include "Widgets/SCompoundWidget.h"

class FProtoBoyDocMap;
class FUICommandList;
class SBox;
class UProtoBoyDocMapGraph;
class UProtoBoyDocMapGraphNode;
struct FProtoBoyDocNode;

/**
 * The doc map: the project's docs by layer, as a Behavior Tree style graph with the Game doc (titled with the project's
 * name) at the top. Drag off a doc's bottom bar to add a doc under it: the menu offers every kind (Design, Tech, Content)
 * and existing notes to link. Wire a doc under another to move it; Delete
 * (after asking) sends a doc to the trash. Every change is written into the docs (frontmatter, new files); the graph is a
 * view rebuilt from FProtoBoyDocMap. Edits made in Obsidian show up after Refresh.
 */
class SProtoBoyDocMapEditor : public SCompoundWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SProtoBoyDocMapEditor) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SProtoBoyDocMapEditor() override;

	// FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("SProtoBoyDocMapEditor"); }

	// Called by the graph (schema and node widgets).
	/** Put a doc under another (empty = back to its layer's default place), from a wire edit. */
	void ReparentFromGraph(const FString& Id, const FString& NewParentId);
	/** False when a search is active and the doc doesn't match it. */
	bool MatchesFilter(const FString& Id) const;

private:
	/** No Game doc yet: set up from a template, or create it on its own. */
	TSharedRef<SWidget> MakeSetupRow();
	/** An old Vision and Overview: offer to combine them into one Game doc. */
	TSharedRef<SWidget> MakeCombineRow();
	TSharedRef<SWidget> MakeToolbar();
	TSharedRef<SWidget> MakeGraph();

	/**
	 * The add menu for docs under ParentId: a title box, the kinds that fit under that doc, and existing notes to link.
	 * OnPicked runs after an entry is used (e.g. to close the graph's action menu).
	 */
	TSharedRef<SWidget> MakeAddMenu(const FString& ParentId, TSharedPtr<SWidget>& OutFocus, TFunction<void()> OnPicked);
	/**
	 * What can be added under a doc: every addable kind, since no rules limit which layer sits under which. The kinds that usually go under this layer come first; Enter uses the first.
	 */
	TArray<EProtoBoyDocKind> GetKindsUnder(const FProtoBoyDocNode& Parent) const;

	// Graph events
	void OnGraphSelectionChanged(const TSet<UObject*>& Selection);
	void OnNodeDoubleClicked(UEdGraphNode* Node);
	FActionMenuContent OnCreateActionMenu(UEdGraph* InGraph, const FVector2f& Location, const TArray<UEdGraphPin*>& DraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed OnClosed);
	FActionMenuContent OnCreateNodeOrPinMenu(UEdGraph* InGraph, const UEdGraphNode* Node, const UEdGraphPin* Pin, FMenuBuilder* Menu, bool bReadOnly);

	/** Rebuild the graph and the details pane on the next frame. Bound to the map's OnChanged. */
	void ScheduleRebuild();
	bool bRebuildPending = false;
	void RebuildGraph();
	void RebuildDetails();
	TSharedRef<SWidget> MakeDetails(const FProtoBoyDocNode& Doc);
	TSharedRef<SWidget> MakeLabel(const FText& Text);
	TSharedRef<SWidget> MakeDocLink(const FProtoBoyDocNode& Doc);
	void ZoomToFitSoon();

	/** Create a doc of Kind titled Title under ParentId. */
	void AddDoc(EProtoBoyDocKind Kind, const FString& ParentId, const FString& Title);
	void LinkExistingDoc(const FString& Path, EProtoBoyDocLayer Layer, const FString& ParentId);
	/** Asks, then deletes the doc (the provider moves it to its trash). */
	void DeleteDocWithConfirm(const FString& Id);
	bool CanDeleteSelected() const;
	/** Open in Obsidian when connected, else in the system's Markdown app. */
	void OpenDoc(const FString& Id);
	FReply OnCreateGameClicked();
	FReply OnCombineClicked();
	/** Opens the Set Up Docs from Template window. */
	FReply OnSetupFromTemplateClicked();
	FReply OnRefreshClicked();
	void Select(const FString& Id);
	/** Show a result line under the graph (error or success). */
	void SetStatus(const FText& Text, bool bError);

	static FProtoBoyDocMap* GetMap();

	TObjectPtr<UProtoBoyDocMapGraph> Graph;
	TObjectPtr<UProtoBoyDocMapGraphNode> RootNode;
	TMap<FString, TObjectPtr<UProtoBoyDocMapGraphNode>> NodesById;
	TSharedPtr<SGraphEditor> GraphEditor;
	TSharedPtr<FUICommandList> GraphCommands;
	TSharedPtr<SBox> DetailsBox;
	FString SelectedId;
	FText FilterText;

	FText StatusText;
	bool bStatusIsError = false;
	FDelegateHandle ChangedHandle;
	/** Set while we rebuild, so selection changes made by the rebuild aren't treated as the user's. */
	bool bRebuilding = false;
};
