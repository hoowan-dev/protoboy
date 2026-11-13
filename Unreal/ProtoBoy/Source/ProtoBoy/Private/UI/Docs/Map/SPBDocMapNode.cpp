#include "UI/Docs/Map/SPBDocMapNode.h"

#include "Docs/PBDocMap.h"
#include "SGraphNodeAI.h"
#include "SGraphPanel.h"
#include "Styling/AppStyle.h"
#include "UI/Docs/Map/PBDocMapGraph.h"
#include "UI/Docs/Map/SPBDocMapEditor.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace ProtoBoyDocMapNode
{
	const FLinearColor BorderColor(0.08f, 0.08f, 0.08f);
}

FLinearColor SProtoBoyDocMapNode::GetLayerColor(EProtoBoyDocLayer Layer)
{
	// From the Behavior Tree editor's palette.
	switch (Layer)
	{
	case EProtoBoyDocLayer::Game: return FLinearColor(0.3f, 0.22f, 0.0f);
	case EProtoBoyDocLayer::Design: return FLinearColor(0.42f, 0.17f, 0.02f);
	case EProtoBoyDocLayer::Tech: return FLinearColor(0.24f, 0.055f, 0.715f);
	case EProtoBoyDocLayer::Content: return FLinearColor(0.0f, 0.28f, 0.35f);
	}
	return FLinearColor(0.1f, 0.1f, 0.1f);
}

void SProtoBoyDocMapNode::Construct(const FArguments& InArgs, UProtoBoyDocMapGraphNode* InNode)
{
	SetCursor(EMouseCursor::CardinalCross);
	GraphNode = InNode;
	UpdateGraphNode();
}

UProtoBoyDocMapGraphNode* SProtoBoyDocMapNode::GetDocNode() const
{
	return Cast<UProtoBoyDocMapGraphNode>(GraphNode);
}

void SProtoBoyDocMapNode::UpdateGraphNode()
{
	InputPins.Empty();
	OutputPins.Empty();
	RightNodeBox.Reset();
	LeftNodeBox.Reset();

	const bool bRoot = GetDocNode() && GetDocNode()->IsRoot();

	GetOrAddSlot(ENodeZone::Center)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(NodeWidth)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("Graph.StateNode.Body"))
				.BorderBackgroundColor(ProtoBoyDocMapNode::BorderColor)
				.ColorAndOpacity(this, &SProtoBoyDocMapNode::GetOpacity)
				.Padding(0.f)
				[
					SNew(SVerticalBox)

					// Input bar (the doc this one sits under)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBox)
						.MinDesiredHeight(8.f)
						[
							SAssignNew(LeftNodeBox, SVerticalBox)
						]
					]

					// Body
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(8.f, 0.f)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("BTEditor.Graph.BTNode.Body"))
						.BorderBackgroundColor(this, &SProtoBoyDocMapNode::GetBodyColor)
						.Padding(FMargin(8.f, 6.f))
						.Visibility(EVisibility::SelfHitTestInvisible)
						[
							SNew(SVerticalBox)

							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(0.f, 0.f, 6.f, 0.f)
								[
									SNew(SImage)
									.Image(FAppStyle::GetBrush(bRoot ? "BTEditor.Graph.BTNode.Root.Icon" : "Icons.Documentation"))
									.DesiredSizeOverride(FVector2D(bRoot ? 20.f : 16.f))
								]

								+ SHorizontalBox::Slot()
								.FillWidth(1.f)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Text_Lambda([this]() { return GraphNode ? GraphNode->GetNodeTitle(ENodeTitleType::FullTitle) : FText::GetEmpty(); })
									.TextStyle(FAppStyle::Get(), "Graph.StateNode.NodeTitle")
									.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
								]
							]

							// Layer
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.f, 2.f, 0.f, 0.f)
							[
								SNew(STextBlock)
								.Text(this, &SProtoBoyDocMapNode::GetLayerText)
								.TextStyle(FAppStyle::Get(), "SmallText")
								.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.6f))
							]

							// First written line
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.f, 4.f, 0.f, 0.f)
							[
								SNew(STextBlock)
								.Text(this, &SProtoBoyDocMapNode::GetSummary)
								.Visibility_Lambda([this]()
								{
									const TSharedPtr<SGraphPanel> Panel = GetOwnerPanel();
									const bool bDetailed = !Panel.IsValid() || Panel->GetCurrentLOD() > EGraphRenderingLOD::LowDetail;
									return bDetailed && !GetSummary().IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
								})
								.AutoWrapText(true)
								.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.85f))
							]
						]
					]

					// Output bar (docs under this one)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBox)
						.MinDesiredHeight(8.f)
						[
							SAssignNew(RightNodeBox, SVerticalBox)
						]
					]
				]
			]
		];

	CreatePinWidgets();
}

void SProtoBoyDocMapNode::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
{
	PinToAdd->SetOwner(SharedThis(this));
	const bool bInput = PinToAdd->GetDirection() == EGPD_Input;
	(bInput ? LeftNodeBox : RightNodeBox)->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		[
			PinToAdd
		];
	(bInput ? InputPins : OutputPins).Add(PinToAdd);
}

TSharedPtr<SGraphPin> SProtoBoyDocMapNode::CreatePinWidget(UEdGraphPin* Pin) const
{
	// The Behavior Tree pin: a full-width bar that lights up on hover.
	return SNew(SGraphPinAI, Pin);
}

FSlateColor SProtoBoyDocMapNode::GetBodyColor() const
{
	const UProtoBoyDocMapGraphNode* Node = GetDocNode();
	const FProtoBoyDocNode* Doc = Node ? Node->GetDoc() : nullptr;
	return Doc ? GetLayerColor(Doc->Layer) : FLinearColor(0.1f, 0.1f, 0.1f);
}

FLinearColor SProtoBoyDocMapNode::GetOpacity() const
{
	const UProtoBoyDocMapGraphNode* Node = GetDocNode();
	const TSharedPtr<SProtoBoyDocMapEditor> Editor = Node ? Node->GetEditor() : nullptr;
	const bool bMatches = !Editor.IsValid() || Node->IsRoot() || Editor->MatchesFilter(Node->DocId);
	return FLinearColor(1.f, 1.f, 1.f, bMatches ? 1.f : 0.3f);
}

FText SProtoBoyDocMapNode::GetLayerText() const
{
	const UProtoBoyDocMapGraphNode* Node = GetDocNode();
	const FProtoBoyDocNode* Doc = Node ? Node->GetDoc() : nullptr;
	if (!Doc)
	{
		return FText::GetEmpty();
	}
	// Tech without a Design doc is shared tech; say so, since it reads differently from a feature's Tech doc.
	return Doc->Layer == EProtoBoyDocLayer::Tech && Doc->ImplementsId.IsEmpty()
		? ProtoBoyDocLayers::GetKindName(EProtoBoyDocKind::SharedTech)
		: ProtoBoyDocLayers::GetDisplayName(Doc->Layer);
}

FText SProtoBoyDocMapNode::GetSummary() const
{
	const UProtoBoyDocMapGraphNode* Node = GetDocNode();
	const FProtoBoyDocNode* Doc = Node ? Node->GetDoc() : nullptr;
	return Doc ? FText::FromString(Doc->Summary) : FText::GetEmpty();
}
