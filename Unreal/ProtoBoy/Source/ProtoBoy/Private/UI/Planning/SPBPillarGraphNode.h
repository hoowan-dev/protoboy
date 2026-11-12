#pragma once

#include "SGraphNode.h"

class UProtoBoyPillarGraphNode;

/**
 * A pillar node drawn like a Behavior Tree node: input bar on top, colored body (name, kind, vision), output bar
 * below. Body color follows the pillar's Kind. Nodes that don't match the editor's search are dimmed.
 */
class SProtoBoyPillarGraphNode : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SProtoBoyPillarGraphNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UProtoBoyPillarGraphNode* InNode);

	virtual void UpdateGraphNode() override;
	virtual void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override;
	virtual TSharedPtr<SGraphPin> CreatePinWidget(UEdGraphPin* Pin) const override;

	/** Width every node is laid out with (the editor's auto layout depends on it). */
	static constexpr float NodeWidth = 220.f;

private:
	UProtoBoyPillarGraphNode* GetPillarNode() const;
	FSlateColor GetBorderColor() const;
	FSlateColor GetBodyColor() const;
	FLinearColor GetOpacity() const;
	const FSlateBrush* GetIcon() const;
	FText GetKind() const;
	FText GetVision() const;
};
