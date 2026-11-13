#include "UI/Docs/Map/PBDocMapGraph.h"

#include "AIGraphConnectionDrawingPolicy.h"
#include "Docs/PBDocMap.h"
#include "PBSubsystem.h"
#include "UI/Docs/Map/SPBDocMapEditor.h"
#include "UI/Docs/Map/SPBDocMapNode.h"

#define LOCTEXT_NAMESPACE "ProtoBoyDocMapGraph"

const FName UProtoBoyDocMapGraphNode::PinCategory(TEXT("ProtoBoyDoc"));

namespace ProtoBoyDocMapGraph
{
	/** Horizontal space per leaf, and vertical space per level. */
	constexpr float ColumnWidth = SProtoBoyDocMapNode::NodeWidth + 40.f;
	constexpr float LevelHeight = 170.f;

	const FProtoBoyDocMap* GetMap()
	{
		const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
		return Subsystem ? &Subsystem->GetDocMap() : nullptr;
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

	const UProtoBoyDocMapGraphNode* OwningNode(const UEdGraphPin* Pin)
	{
		return Pin ? Cast<UProtoBoyDocMapGraphNode>(Pin->GetOwningNode()) : nullptr;
	}

	/** Move a doc through the editor, so errors show there. Empty NewParentId = back to its default parent. */
	void Reparent(const UProtoBoyDocMapGraphNode* Child, const FString& NewParentId)
	{
		if (!Child || Child->IsRoot())
		{
			return;
		}
		if (const TSharedPtr<SProtoBoyDocMapEditor> Editor = Child->GetEditor())
		{
			Editor->ReparentFromGraph(Child->DocId, NewParentId);
		}
	}
}

// --- Graph --------------------------------------------------------------------------------------------------------

void UProtoBoyDocMapGraph::LinkAndLayout(UProtoBoyDocMapGraphNode* Root, const TMap<FString, TObjectPtr<UProtoBoyDocMapGraphNode>>& NodesById)
{
	for (UEdGraphNode* Node : Nodes)
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			Pin->BreakAllPinLinks();
		}
	}
	const FProtoBoyDocMap* Map = ProtoBoyDocMapGraph::GetMap();
	if (!Root || !Map)
	{
		return;
	}

	auto GetChildIds = [Map, &NodesById](const FString& Id)
	{
		TArray<FString> Ids;
		for (const FProtoBoyDocNode* Child : Map->GetChildren(Id))
		{
			if (NodesById.Contains(Child->Id))
			{
				Ids.Add(Child->Id);
			}
		}
		return Ids;
	};

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
	CountLeaves(Root->DocId);

	TFunction<void(UProtoBoyDocMapGraphNode*, int32, float)> Place = [&](UProtoBoyDocMapGraphNode* Node, int32 Depth, float Left)
	{
		const float Width = Leaves.FindRef(Node->DocId) * ProtoBoyDocMapGraph::ColumnWidth;
		Node->NodePosX = FMath::RoundToInt(Left + (Width - SProtoBoyDocMapNode::NodeWidth) * 0.5f);
		Node->NodePosY = FMath::RoundToInt(Depth * ProtoBoyDocMapGraph::LevelHeight);

		float ChildLeft = Left;
		for (const FString& ChildId : GetChildIds(Node->DocId))
		{
			UProtoBoyDocMapGraphNode* ChildNode = NodesById.FindRef(ChildId);
			Node->GetOutputPin()->MakeLinkTo(ChildNode->GetInputPin());
			Place(ChildNode, Depth + 1, ChildLeft);
			ChildLeft += Leaves.FindRef(ChildId) * ProtoBoyDocMapGraph::ColumnWidth;
		}
	};
	Place(Root, 0, 0.f);
}

// --- Node ---------------------------------------------------------------------------------------------------------

const FProtoBoyDocNode* UProtoBoyDocMapGraphNode::GetDoc() const
{
	const FProtoBoyDocMap* Map = ProtoBoyDocMapGraph::GetMap();
	return Map ? Map->Find(DocId) : nullptr;
}

UEdGraphPin* UProtoBoyDocMapGraphNode::GetInputPin() const
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

UEdGraphPin* UProtoBoyDocMapGraphNode::GetOutputPin() const
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

TSharedPtr<SProtoBoyDocMapEditor> UProtoBoyDocMapGraphNode::GetEditor() const
{
	const UProtoBoyDocMapGraph* Graph = Cast<UProtoBoyDocMapGraph>(GetGraph());
	return Graph ? Graph->Editor.Pin() : nullptr;
}

void UProtoBoyDocMapGraphNode::AllocateDefaultPins()
{
	if (!IsRoot())
	{
		CreatePin(EGPD_Input, PinCategory, TEXT("Parent"));
	}
	CreatePin(EGPD_Output, PinCategory, TEXT("Children"));
}

FText UProtoBoyDocMapGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	const FProtoBoyDocNode* Doc = GetDoc();
	return FText::FromString(Doc ? Doc->Title : DocId);
}

FText UProtoBoyDocMapGraphNode::GetTooltipText() const
{
	const FProtoBoyDocNode* Doc = GetDoc();
	if (!Doc)
	{
		return FText::GetEmpty();
	}
	return FText::Format(LOCTEXT("NodeTip", "{0}\n{1}\n\n{2}\nDouble-click to open. Drag from the bottom bar to a doc's top bar to put it under this one."),
		FText::FromString(Doc->Path), ProtoBoyDocLayers::GetDisplayName(Doc->Layer), ProtoBoyDocLayers::GetDescription(Doc->Layer));
}

TSharedPtr<SGraphNode> UProtoBoyDocMapGraphNode::CreateVisualWidget()
{
	return SNew(SProtoBoyDocMapNode, this);
}

// --- Schema -------------------------------------------------------------------------------------------------------

const FPinConnectionResponse UProtoBoyDocMapGraphSchema::CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const
{
	const UEdGraphPin* ParentPin = nullptr;
	const UEdGraphPin* ChildPin = nullptr;
	if (!ProtoBoyDocMapGraph::SplitPins(A, B, ParentPin, ChildPin))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("SameDirection", "Connect a doc's bottom bar to another doc's top bar."));
	}

	const UProtoBoyDocMapGraphNode* Parent = ProtoBoyDocMapGraph::OwningNode(ParentPin);
	const UProtoBoyDocMapGraphNode* Child = ProtoBoyDocMapGraph::OwningNode(ChildPin);
	const FProtoBoyDocMap* Map = ProtoBoyDocMapGraph::GetMap();
	if (!Parent || !Child || Child->IsRoot() || !Map)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, FText::GetEmpty());
	}
	if (Parent == Child || Map->IsInSubtree(Parent->DocId, Child->DocId))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("Loop", "A doc can't go under itself or a doc below it."));
	}

	// The child keeps a single parent: connecting replaces its current one.
	return FPinConnectionResponse(ChildPin == A ? CONNECT_RESPONSE_BREAK_OTHERS_A : CONNECT_RESPONSE_BREAK_OTHERS_B,
		FText::Format(LOCTEXT("MoveUnder", "Put {0} under {1}"), Child->GetNodeTitle(ENodeTitleType::FullTitle), Parent->GetNodeTitle(ENodeTitleType::FullTitle)));
}

bool UProtoBoyDocMapGraphSchema::TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const
{
	if (CanCreateConnection(A, B).Response == CONNECT_RESPONSE_DISALLOW)
	{
		return false;
	}
	const UEdGraphPin* ParentPin = nullptr;
	const UEdGraphPin* ChildPin = nullptr;
	ProtoBoyDocMapGraph::SplitPins(A, B, ParentPin, ChildPin);

	// Don't link pins here: the editor rebuilds the graph from the map once the doc is written.
	ProtoBoyDocMapGraph::Reparent(ProtoBoyDocMapGraph::OwningNode(ChildPin), ProtoBoyDocMapGraph::OwningNode(ParentPin)->DocId);
	return true;
}

void UProtoBoyDocMapGraphSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotifcation) const
{
	// Unhooking a doc sends it back to where its layer puts it by default (the Game doc, or its Design doc for a feature's Tech doc).
	if (TargetPin.Direction == EGPD_Input)
	{
		ProtoBoyDocMapGraph::Reparent(ProtoBoyDocMapGraph::OwningNode(&TargetPin), FString());
		return;
	}
	const TArray<UEdGraphPin*> Links = TargetPin.LinkedTo;
	for (const UEdGraphPin* Linked : Links)
	{
		ProtoBoyDocMapGraph::Reparent(ProtoBoyDocMapGraph::OwningNode(Linked), FString());
	}
}

void UProtoBoyDocMapGraphSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const
{
	const UEdGraphPin* ParentPin = nullptr;
	const UEdGraphPin* ChildPin = nullptr;
	if (ProtoBoyDocMapGraph::SplitPins(SourcePin, TargetPin, ParentPin, ChildPin))
	{
		ProtoBoyDocMapGraph::Reparent(ProtoBoyDocMapGraph::OwningNode(ChildPin), FString());
	}
}

FLinearColor UProtoBoyDocMapGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return FLinearColor::White;
}

FConnectionDrawingPolicy* UProtoBoyDocMapGraphSchema::CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor,
	const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj) const
{
	// Behavior Tree wiring: straight lines with arrows from parent to child.
	return new FAIGraphConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
}

#undef LOCTEXT_NAMESPACE
