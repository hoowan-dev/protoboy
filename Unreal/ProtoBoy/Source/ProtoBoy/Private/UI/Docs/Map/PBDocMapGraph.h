#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphSchema.h"
#include "PBDocMapGraph.generated.h"

class SProtoBoyDocMapEditor;
class UProtoBoyDocMapGraphNode;
struct FProtoBoyDocNode;

/**
 * The doc map shown as a Behavior Tree style graph: the Vision at the top, each doc below the doc it sits under.
 * A transient view of FProtoBoyDocMap: SProtoBoyDocMapEditor rebuilds it after every change, and wiring a node under
 * another goes back through the map (which writes the doc's frontmatter).
 */
UCLASS(Transient)
class UProtoBoyDocMapGraph : public UEdGraph
{
	GENERATED_BODY()

public:
	/** The editor showing this graph; the schema routes wiring through it. */
	TWeakPtr<SProtoBoyDocMapEditor> Editor;

	/**
	 * Link each node's children pin to its children and lay the tree out top-down from Root: each leaf gets a column
	 * and a parent sits centered over its children. Clears existing links first.
	 */
	void LinkAndLayout(UProtoBoyDocMapGraphNode* Root, const TMap<FString, TObjectPtr<UProtoBoyDocMapGraphNode>>& NodesById);
};

/** One doc. Input pin on top (the doc it sits under), output pin below (docs under it); the Vision has no input. */
UCLASS(Transient)
class UProtoBoyDocMapGraphNode : public UEdGraphNode
{
	GENERATED_BODY()

public:
	static const FName PinCategory;

	UPROPERTY()
	FString DocId;

	/** The Vision (the map's root). Set before AllocateDefaultPins. */
	UPROPERTY()
	bool bRootNode = false;

	bool IsRoot() const { return bRootNode; }
	/** The doc this node shows, or null if it's gone from the map. */
	const FProtoBoyDocNode* GetDoc() const;
	UEdGraphPin* GetInputPin() const;
	UEdGraphPin* GetOutputPin() const;
	TSharedPtr<SProtoBoyDocMapEditor> GetEditor() const;

	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual bool GetCanRenameNode() const override { return false; }
	virtual bool CanUserDeleteNode() const override { return false; }
	virtual bool CanDuplicateNode() const override { return false; }
	virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;
};

/** Wiring: a doc's bottom bar to another doc's top bar puts the second under the first. One parent each; no loops. */
UCLASS()
class UProtoBoyDocMapGraphSchema : public UEdGraphSchema
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
