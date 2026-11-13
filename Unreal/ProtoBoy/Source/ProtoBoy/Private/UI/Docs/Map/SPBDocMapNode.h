#pragma once

#include "SGraphNode.h"

class UProtoBoyDocMapGraphNode;

/**
 * A doc on the map, drawn like a Behavior Tree node: input bar on top, a body colored by layer (title, layer, the
 * doc's first written line), output bar below. Nodes that don't match the editor's search are dimmed.
 */
class SProtoBoyDocMapNode : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SProtoBoyDocMapNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UProtoBoyDocMapGraphNode* InNode);

	virtual void UpdateGraphNode() override;
	virtual void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override;
	virtual TSharedPtr<SGraphPin> CreatePinWidget(UEdGraphPin* Pin) const override;

	/** Width every node is laid out with (the auto layout depends on it). */
	static constexpr float NodeWidth = 220.f;

private:
	UProtoBoyDocMapGraphNode* GetDocNode() const;
	FSlateColor GetBodyColor() const;
	FLinearColor GetOpacity() const;
	FText GetLayerText() const;
	FText GetSummary() const;
};
