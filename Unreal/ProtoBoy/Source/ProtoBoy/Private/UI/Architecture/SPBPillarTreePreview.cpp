#include "UI/Architecture/SPBPillarTreePreview.h"

#include "Architecture/PBPillars.h"
#include "GraphEditor.h"
#include "UI/Architecture/PBPillarGraph.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "SProtoBoyPillarTreePreview"

void SProtoBoyPillarTreePreview::Construct(const FArguments& InArgs)
{
	Graph = NewObject<UProtoBoyPillarGraph>(GetTransientPackage(), NAME_None, RF_Transient);
	Graph->Schema = UProtoBoyPillarGraphSchema::StaticClass();
	Graph->bPreview = true;

	FGraphAppearanceInfo Appearance;
	Appearance.CornerText = LOCTEXT("CornerText", "PREVIEW");

	ChildSlot
	[
		SAssignNew(GraphEditor, SGraphEditor)
		.GraphToEdit(Graph)
		.Appearance(Appearance)
		.IsEditable(false)
		.ShowGraphStateOverlay(false)
	];
}

void SProtoBoyPillarTreePreview::SetPillars(const TArray<FProtoBoyPillar>& Pillars)
{
	// Rebuilt from scratch: a preview only changes when a different tree is picked.
	const TArray<UEdGraphNode*> OldNodes = Graph->Nodes;
	for (UEdGraphNode* Node : OldNodes)
	{
		Graph->RemoveNode(Node);
	}
	Graph->PreviewPillars = Pillars;

	TMap<FString, TObjectPtr<UProtoBoyPillarGraphNode>> NodesById;
	UProtoBoyPillarGraphNode* Root = nullptr;
	for (int32 Index = 0; Index < Pillars.Num(); ++Index)
	{
		const FProtoBoyPillar& Pillar = Pillars[Index];
		UProtoBoyPillarGraphNode* Node = NewObject<UProtoBoyPillarGraphNode>(Graph, NAME_None, RF_Transient);
		Node->PillarId = Pillar.Id;
		Node->bRootNode = Index == 0;
		Node->bCanRenameNode = false;
		Node->CreateNewGuid();
		Node->AllocateDefaultPins();
		Graph->AddNode(Node, false, false);
		NodesById.Add(Pillar.Id, Node);
		if (Index == 0)
		{
			Root = Node;
		}
	}

	Graph->LinkAndLayout(Root, NodesById, [&Pillars](const FString& ParentId)
	{
		TArray<const FProtoBoyPillar*> Children;
		for (const FProtoBoyPillar& Pillar : Pillars)
		{
			if (&Pillar != &Pillars[0] && Pillar.ParentId == ParentId)
			{
				Children.Add(&Pillar);
			}
		}
		Children.StableSort([](const FProtoBoyPillar& A, const FProtoBoyPillar& B) { return A.Order < B.Order; });

		TArray<FString> Ids;
		for (const FProtoBoyPillar* Child : Children)
		{
			Ids.Add(Child->Id);
		}
		return Ids;
	});
	Graph->NotifyGraphChanged();

	// Node widgets are created on the panel's next update; fit once they have a size.
	RegisterActiveTimer(0.1f, FWidgetActiveTimerDelegate::CreateLambda([this](double, float)
	{
		if (GraphEditor.IsValid())
		{
			GraphEditor->ZoomToFit(false);
		}
		return EActiveTimerReturnType::Stop;
	}));
}

void SProtoBoyPillarTreePreview::AddReferencedObjects(FReferenceCollector& Collector)
{
	// The graph owns its nodes (UEdGraph::Nodes), so this keeps them alive too.
	Collector.AddReferencedObject(Graph);
}

#undef LOCTEXT_NAMESPACE
