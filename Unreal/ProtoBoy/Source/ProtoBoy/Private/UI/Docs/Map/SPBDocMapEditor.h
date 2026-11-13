#pragma once

#include "Docs/PBDocLayers.h"
#include "GraphEditor.h"
#include "UObject/GCObject.h"
#include "Widgets/SCompoundWidget.h"

class FProtoBoyDocMap;
class FUICommandList;
class SBox;
class SEditableTextBox;
class UProtoBoyDocMapGraph;
class UProtoBoyDocMapGraphNode;
struct FProtoBoyDocNode;

/**
 * The doc map: the project's docs by layer, as a Behavior Tree style graph with the Vision at the top. Add docs from
 * their layer's template, add a Design doc's Tech doc, put existing notes on the map, and wire a doc under another.
 * Every change is written into the docs (frontmatter, new files); the graph is a view rebuilt from FProtoBoyDocMap.
 * Edits made in Obsidian show up after Refresh.
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
	TSharedRef<SWidget> MakeSetupRow();
	TSharedRef<SWidget> MakeToolbar();
	TSharedRef<SWidget> MakeAddMenu();
	TSharedRef<SWidget> MakeMapExistingMenu();
	TSharedRef<SWidget> MakeGraph();

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

	/** Add a doc titled with the toolbar's title box under ParentId (empty = its layer's default place). */
	void AddDoc(EProtoBoyDocLayer Layer, const FString& ParentId);
	void AddTechDoc(const FString& DesignId);
	void MapExistingDoc(const FString& Path, EProtoBoyDocLayer Layer);
	/** Open in Obsidian when connected, else in the system's Markdown app. */
	void OpenDoc(const FString& Id);
	FReply OnCreateVisionClicked();
	FReply OnRefreshClicked();
	/** The selected doc, or the Overview when nothing is selected: where Add puts new docs. */
	FString GetAddParentId() const;
	FString GetNewTitle() const;
	bool CanAddTechDoc(const FString& Id) const;
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
	TSharedPtr<SEditableTextBox> TitleBox;
	FString SelectedId;
	FText FilterText;
	bool bZoomedToFit = false;

	FText StatusText;
	bool bStatusIsError = false;
	FDelegateHandle ChangedHandle;
	/** Set while we rebuild, so selection changes made by the rebuild aren't treated as the user's. */
	bool bRebuilding = false;
};
