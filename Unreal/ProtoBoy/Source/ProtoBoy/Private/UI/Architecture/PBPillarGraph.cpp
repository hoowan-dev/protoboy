#include "UI/Architecture/PBPillarGraph.h"

#include "AIGraphConnectionDrawingPolicy.h"
#include "PBSubsystem.h"
#include "Architecture/PBPillars.h"
#include "UI/Architecture/SPBPillarGraphNode.h"
#include "UI/Architecture/SPBPillarTreeEditor.h"

#define LOCTEXT_NAMESPACE "ProtoBoyPillarGraph"

const FName UProtoBoyPillarGraphNode::PinCategory(TEXT("ProtoBoyPillar"));

namespace ProtoBoyPillarGraph
{
	const FProtoBoyPillarService* GetService()
	{
		const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
		return Subsystem ? &Subsystem->GetPillars() : nullptr;
	}

	/** The parent-side (output) and child-side (input) pins of a connection, in either order. */
	bool SplitPins(const UEdGraphPin* A, const UEdGraphPin* B, const UEdGraphPin*& OutParentPin, const UEdGraphPin*& OutChildPin)
	{
		if (!A || !B || A->Direction == B->Direction)
		{
			return false;
		}
		OutParentPin = A->Direction == EGPD_Output ? A : B;
		OutChildPin = A->Direction == EGPD_Output ? B : A;
		return true;
	}

	const UProtoBoyPillarGraphNode* OwningNode(const UEdGraphPin* Pin)
	{
		return Pin ? Cast<UProtoBoyPillarGraphNode>(Pin->GetOwningNode()) : nullptr;
	}

	/** Reparent through the editor (so errors show there), or straight on the service. */
	void Reparent(const UProtoBoyPillarGraphNode* Child, const FString& NewParentId)
	{
		if (!Child || Child->IsRoot())
		{
			return;
		}
		if (const TSharedPtr<SProtoBoyPillarTreeEditor> Editor = Child->GetEditor())
		{
			Editor->ReparentFromGraph(Child->PillarId, NewParentId);
		}
	}
}

// --- Node ---------------------------------------------------------------------------------------------------------

UEdGraphPin* UProtoBoyPillarGraphNode::GetInputPin() const
{
	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin->Direction == EGPD_Input)
		{
			return Pin;
		}
	}
	return nullptr;
}

UEdGraphPin* UProtoBoyPillarGraphNode::GetOutputPin() const
{
	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin->Direction == EGPD_Output)
		{
			return Pin;
		}
	}
	return nullptr;
}

TSharedPtr<SProtoBoyPillarTreeEditor> UProtoBoyPillarGraphNode::GetEditor() const
{
	const UProtoBoyPillarGraph* Graph = Cast<UProtoBoyPillarGraph>(GetGraph());
	return Graph ? Graph->Editor.Pin() : nullptr;
}

void UProtoBoyPillarGraphNode::AllocateDefaultPins()
{
	if (!IsRoot())
	{
		CreatePin(EGPD_Input, PinCategory, TEXT("Parent"));
	}
	CreatePin(EGPD_Output, PinCategory, TEXT("Children"));
}

FText UProtoBoyPillarGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	const FProtoBoyPillarService* Service = ProtoBoyPillarGraph::GetService();
	const FProtoBoyPillar* Pillar = Service ? Service->Find(PillarId) : nullptr;
	return Pillar ? FText::FromString(Pillar->Name) : FText::FromString(PillarId);
}

FText UProtoBoyPillarGraphNode::GetTooltipText() const
{
	if (IsRoot())
	{
		return LOCTEXT("RootTip", "The game, named after the project: the root of the pillar tree. Every pillar sits below it; connect one to it to make it top-level. Double-click to open the Architecture doc.");
	}
	const FProtoBoyPillarService* Service = ProtoBoyPillarGraph::GetService();
	return Service
		? FText::Format(LOCTEXT("NodeTip", "{0}  ({1})\nDrag from the bottom bar to a pillar's top bar to nest it. F2 to rename, Delete to remove."),
			FText::FromString(Service->GetDisplayPath(PillarId)), FText::FromString(PillarId))
		: FText::GetEmpty();
}

TSharedPtr<SGraphNode> UProtoBoyPillarGraphNode::CreateVisualWidget()
{
	return SNew(SProtoBoyPillarGraphNode, this);
}

// --- Schema -------------------------------------------------------------------------------------------------------

const FPinConnectionResponse UProtoBoyPillarGraphSchema::CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const
{
	const UEdGraphPin* ParentPin = nullptr;
	const UEdGraphPin* ChildPin = nullptr;
	if (!ProtoBoyPillarGraph::SplitPins(A, B, ParentPin, ChildPin))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("SameDirection", "Connect a pillar's bottom bar to another pillar's top bar."));
	}

	const UProtoBoyPillarGraphNode* Parent = ProtoBoyPillarGraph::OwningNode(ParentPin);
	const UProtoBoyPillarGraphNode* Child = ProtoBoyPillarGraph::OwningNode(ChildPin);
	const FProtoBoyPillarService* Service = ProtoBoyPillarGraph::GetService();
	if (!Parent || !Child || Child->IsRoot() || !Service)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, FText::GetEmpty());
	}
	if (Parent == Child || (!Parent->IsRoot() && Service->IsInSubtree(Parent->PillarId, Child->PillarId)))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("Cycle", "A pillar can't be moved under itself or one of its own children."));
	}

	// The child keeps a single parent: connecting replaces its current one.
	const FText Message = Parent->IsRoot()
		? FText::Format(LOCTEXT("MakeTopLevel", "Make {0} a top-level pillar"), Child->GetNodeTitle(ENodeTitleType::FullTitle))
		: FText::Format(LOCTEXT("MoveUnder", "Move {0} under {1}"), Child->GetNodeTitle(ENodeTitleType::FullTitle), Parent->GetNodeTitle(ENodeTitleType::FullTitle));
	return FPinConnectionResponse(ChildPin == A ? CONNECT_RESPONSE_BREAK_OTHERS_A : CONNECT_RESPONSE_BREAK_OTHERS_B, Message);
}

bool UProtoBoyPillarGraphSchema::TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const
{
	if (CanCreateConnection(A, B).Response == CONNECT_RESPONSE_DISALLOW)
	{
		return false;
	}
	const UEdGraphPin* ParentPin = nullptr;
	const UEdGraphPin* ChildPin = nullptr;
	ProtoBoyPillarGraph::SplitPins(A, B, ParentPin, ChildPin);

	// Don't link pins here: the editor rebuilds the graph from the service once the move is saved. A root parent
	// passes the root's id, which the service treats like any other parent.
	ProtoBoyPillarGraph::Reparent(ProtoBoyPillarGraph::OwningNode(ChildPin), ProtoBoyPillarGraph::OwningNode(ParentPin)->PillarId);
	return true;
}

void UProtoBoyPillarGraphSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotifcation) const
{
	// Unparenting makes a pillar top-level (under the root): the root is the only pillar without a parent.
	if (TargetPin.Direction == EGPD_Input)
	{
		ProtoBoyPillarGraph::Reparent(ProtoBoyPillarGraph::OwningNode(&TargetPin), FString());
		return;
	}
	const UProtoBoyPillarGraphNode* Owner = ProtoBoyPillarGraph::OwningNode(&TargetPin);
	if (Owner && !Owner->IsRoot())
	{
		const TArray<UEdGraphPin*> Links = TargetPin.LinkedTo;
		for (const UEdGraphPin* Linked : Links)
		{
			ProtoBoyPillarGraph::Reparent(ProtoBoyPillarGraph::OwningNode(Linked), FString());
		}
	}
}

void UProtoBoyPillarGraphSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const
{
	const UEdGraphPin* ParentPin = nullptr;
	const UEdGraphPin* ChildPin = nullptr;
	if (ProtoBoyPillarGraph::SplitPins(SourcePin, TargetPin, ParentPin, ChildPin) && !ProtoBoyPillarGraph::OwningNode(ParentPin)->IsRoot())
	{
		ProtoBoyPillarGraph::Reparent(ProtoBoyPillarGraph::OwningNode(ChildPin), FString());
	}
}

FLinearColor UProtoBoyPillarGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return FLinearColor::White;
}

FConnectionDrawingPolicy* UProtoBoyPillarGraphSchema::CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor,
	const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj) const
{
	// Behavior Tree wiring: straight lines with arrows from parent to child.
	return new FAIGraphConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
}

#undef LOCTEXT_NAMESPACE
