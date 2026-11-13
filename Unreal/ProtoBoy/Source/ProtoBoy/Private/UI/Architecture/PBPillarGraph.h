#pragma once

#include "CoreMinimal.h"
#include "Architecture/PBPillars.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphSchema.h"
#include "PBPillarGraph.generated.h"

class SProtoBoyPillarTreeEditor;
class UProtoBoyPillarGraphNode;

/**
 * The pillar tree shown as a Behavior Tree style graph: a root node (the game) at the top and one node per pillar,
 * parent above child. The graph is a transient view of FProtoBoyPillarService: SProtoBoyPillarTreeEditor rebuilds it
 * from the service after every change, and graph edits (wiring, renames) go back through the service.
 * A preview graph (bPreview) shows its own PreviewPillars instead, e.g. a template, and is never edited.
 */
UCLASS(Transient)
class UProtoBoyPillarGraph : public UEdGraph
{
	GENERATED_BODY()

public:
	/** The editor showing this graph; schema and node widgets route edits through it. Unset for a preview. */
	TWeakPtr<SProtoBoyPillarTreeEditor> Editor;

	/** Shows PreviewPillars, read-only, instead of the pillar file. */
	bool bPreview = false;
	TArray<FProtoBoyPillar> PreviewPillars;

	/** The pillar a node shows: from PreviewPillars for a preview, else from the pillar service. */
	const FProtoBoyPillar* FindPillar(const FString& Id) const;

	/**
	 * Link each node's children pin to its children and lay the tree out top-down from Root: each leaf gets a column
	 * and a parent sits centered over its children, the way Behavior Trees read. GetChildIds returns a pillar's
	 * children in order. Clears existing links first.
	 */
	void LinkAndLayout(UProtoBoyPillarGraphNode* Root, const TMap<FString, TObjectPtr<UProtoBoyPillarGraphNode>>& NodesById,
		TFunctionRef<TArray<FString>(const FString& ParentId)> GetChildIds);
};

/** One pillar. Input pin on top (its parent), output pin below (its children); the root (the game) has no input. */
UCLASS(Transient)
class UProtoBoyPillarGraphNode : public UEdGraphNode
{
	GENERATED_BODY()

public:
	static const FName PinCategory;

	UPROPERTY()
	FString PillarId;

	/** The root pillar (the game). Set before AllocateDefaultPins. */
	UPROPERTY()
	bool bRootNode = false;

	bool IsRoot() const { return bRootNode; }
	UEdGraphPin* GetInputPin() const;
	UEdGraphPin* GetOutputPin() const;
	TSharedPtr<SProtoBoyPillarTreeEditor> GetEditor() const;

	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual bool GetCanRenameNode() const override { return !IsRoot(); }
	virtual bool CanUserDeleteNode() const override { return !IsRoot(); }
	virtual bool CanDuplicateNode() const override { return false; }
	virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;
};

/** Wiring rules: an output pin (parent) connects to an input pin (child); each pillar has one parent; no cycles. */
UCLASS()
class UProtoBoyPillarGraphSchema : public UEdGraphSchema
{
	GENERATED_BODY()

public:
	virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const override;
	virtual bool TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const override;
	virtual void BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotifcation) const override;
	virtual void BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const override;
	virtual FLinearColor GetPinTypeColor(const FEdGraphPinType& PinType) const override;
	virtual FConnectionDrawingPolicy* CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor,
		const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj) const override;
};
