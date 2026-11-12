#pragma once

#include "GraphEditor.h"
#include "UObject/GCObject.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SCompoundWidget.h"

class FProtoBoyPillarService;
class FUICommandList;
class SBox;
class UProtoBoyPillarGraph;
class UProtoBoyPillarGraphNode;

/**
 * Planning > Pillars: the pillar tree as a Behavior Tree style graph. The game is the root node at the top, each
 * pillar a node below its parent, laid out automatically. Wire a node's bottom bar to another's top bar to nest it,
 * drag off a bottom bar into empty space to add a child, F2 to rename, Delete to remove, right-click for more.
 * The selected pillar's details are edited below the graph, and docs are generated from the tree (with a preview).
 * Edits save to the pillar file immediately; the graph is only a view rebuilt from FProtoBoyPillarService.
 */
class SProtoBoyPillarTreeEditor : public SCompoundWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SProtoBoyPillarTreeEditor) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SProtoBoyPillarTreeEditor() override;

	// FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("SProtoBoyPillarTreeEditor"); }

	// Called by the graph (schema and node widgets).
	/** Move a pillar under a new parent (empty = top level), from a wire edit. */
	void ReparentFromGraph(const FString& Id, const FString& NewParentId);
	/** False when a search is active and the pillar doesn't match it. */
	bool MatchesFilter(const FString& Id) const;
	/** True once for the pillar that should open in rename mode when its node widget is created. */
	bool ConsumePendingRename(const FString& Id);

private:
	TSharedRef<SWidget> MakeToolbar();
	TSharedRef<SWidget> MakeAddMenu();
	TSharedRef<SWidget> MakeGraph();

	// Graph events
	void OnGraphSelectionChanged(const TSet<UObject*>& Selection);
	void OnNodeTextCommitted(const FText& Text, ETextCommit::Type CommitType, UEdGraphNode* Node);
	bool OnNodeVerifyTextCommit(const FText& Text, UEdGraphNode* Node, FText& OutError);
	void OnNodeDoubleClicked(UEdGraphNode* Node);
	FActionMenuContent OnCreateActionMenu(UEdGraph* InGraph, const FVector2f& Location, const TArray<UEdGraphPin*>& DraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed OnClosed);
	FActionMenuContent OnCreateNodeOrPinMenu(UEdGraph* InGraph, const UEdGraphNode* Node, const UEdGraphPin* Pin, FMenuBuilder* Menu, bool bReadOnly);

	void OnFilterTextChanged(const FText& Text);
	void OnFilterTextCommitted(const FText& Text, ETextCommit::Type CommitType);

	/** Rebuild the graph and the details pane on the next frame. Bound to the service's OnChanged. */
	void ScheduleRebuild();
	bool bRebuildPending = false;
	/** Sync graph nodes and links with the service and lay the tree out top-down. */
	void RebuildGraph();
	/** Rebuild the details pane for the selected pillar. */
	void RebuildDetails();
	TSharedRef<SWidget> MakeDetails(const FString& Id);
	TSharedRef<SWidget> MakeLabel(const FText& Text);
	void ZoomToFitSoon();

	void Select(const FString& Id);
	void RequestRename(const FString& Id);
	void AddPillarUnder(const FString& ParentId);
	FReply OnAddClicked(bool bAsChild);
	FReply OnMoveClicked(int32 Delta);
	FReply OnRemoveClicked();
	FReply OnGenerateDocsClicked();
	FReply OnSetupPillarsClicked();
	/** Asks, then removes every pillar below the root. */
	FReply OnClearPillarsClicked();
	bool HasSelectedPillar() const;

	static FProtoBoyPillarService* GetService();

	TObjectPtr<UProtoBoyPillarGraph> Graph;
	TObjectPtr<UProtoBoyPillarGraphNode> RootNode;
	TMap<FString, TObjectPtr<UProtoBoyPillarGraphNode>> NodesById;
	TSharedPtr<SGraphEditor> GraphEditor;
	TSharedPtr<FUICommandList> GraphCommands;
	TSharedPtr<SBox> DetailsBox;
	FString SelectedId;
	FString PendingRenameId;
	FText FilterText;
	bool bZoomedToFit = false;

	/** Options for the details pane's parent picker ("" = top level). */
	TArray<TSharedPtr<FString>> ParentOptions;

	FText EditError;
	FDelegateHandle ChangedHandle;
	/** Set while we rebuild, so selection changes made by the rebuild aren't treated as the user's. */
	bool bRebuilding = false;
};
