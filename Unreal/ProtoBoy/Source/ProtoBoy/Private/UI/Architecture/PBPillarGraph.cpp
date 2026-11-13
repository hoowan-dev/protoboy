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
	/** Horizontal space per leaf, and vertical space per tree level. */
	constexpr float ColumnWidth = SProtoBoyPillarGraphNode::NodeWidth + 40.f;
	constexpr float LevelHeight = 190.f;

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

// --- Graph --------------------------------------------------------------------------------------------------------

const FProtoBoyPillar* UProtoBoyPillarGraph::FindPillar(const FString& Id) const
{
	if (bPreview)
	{
		return PreviewPillars.FindByPredicate([&Id](const FProtoBoyPillar& P) { return P.Id == Id; });
	}
	const FProtoBoyPillarService* Service = ProtoBoyPillarGraph::GetService();
	return Service && !Id.IsEmpty() ? Service->Find(Id) : nullptr;
}

void UProtoBoyPillarGraph::LinkAndLayout(UProtoBoyPillarGraphNode* Root, const TMap<FString, TObjectPtr<UProtoBoyPillarGraphNode>>& NodesById,
	TFunctionRef<TArray<FString>(const FString& ParentId)> GetChildIds)
{
	for (UEdGraphNode* Node : Nodes)
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			Pin->BreakAllPinLinks();
		}
	}
	if (!Root)
	{
		return;
	}

	TMap<FString, int32> Leaves;
	TFunction<int32(const FString&)> CountLeaves = [&](const FString& Id) -> int32
	{
		int32 Count = 0;
		for (const FString& ChildId : GetChildIds(Id))
		{
			Count += CountLeaves(ChildId);
		}
		Count = FMath::Max(1, Count);
		Leaves.Add(Id, Count);
		return Count;
	};
	CountLeaves(Root->PillarId);

	TFunction<void(UProtoBoyPillarGraphNode*, int32, float)> Place = [&](UProtoBoyPillarGraphNode* Node, int32 Depth, float Left)
	{
		const float Width = Leaves.FindRef(Node->PillarId) * ProtoBoyPillarGraph::ColumnWidth;
		Node->NodePosX = FMath::RoundToInt(Left + (Width - SProtoBoyPillarGraphNode::NodeWidth) * 0.5f);
		Node->NodePosY = FMath::RoundToInt(Depth * ProtoBoyPillarGraph::LevelHeight);

		float ChildLeft = Left;
		for (const FString& ChildId : GetChildIds(Node->PillarId))
		{
			UProtoBoyPillarGraphNode* ChildNode = NodesById.FindRef(ChildId);
			if (!ChildNode)
			{
				continue;
			}
			Node->GetOutputPin()->MakeLinkTo(ChildNode->GetInputPin());
			Place(ChildNode, Depth + 1, ChildLeft);
			ChildLeft += Leaves.FindRef(ChildId) * ProtoBoyPillarGraph::ColumnWidth;
		}
	};
	Place(Root, 0, 0.f);
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
	const UProtoBoyPillarGraph* Graph = Cast<UProtoBoyPillarGraph>(GetGraph());
	const FProtoBoyPillar* Pillar = Graph ? Graph->FindPillar(PillarId) : nullptr;
	return Pillar ? FText::FromString(Pillar->Name) : FText::FromString(PillarId);
}

FText UProtoBoyPillarGraphNode::GetTooltipText() const
{
	const UProtoBoyPillarGraph* Graph = Cast<UProtoBoyPillarGraph>(GetGraph());
	if (Graph && Graph->bPreview)
	{
		const FProtoBoyPillar* Pillar = Graph->FindPillar(PillarId);
		return Pillar ? FText::FromString(Pillar->Description) : FText::GetEmpty();
	}
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
	const UProtoBoyPillarGraph* Graph = Parent ? Cast<UProtoBoyPillarGraph>(Parent->GetGraph()) : nullptr;
	if (!Parent || !Child || Child->IsRoot() || !Service || !Graph || Graph->bPreview)
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
