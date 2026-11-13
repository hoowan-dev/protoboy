#include "UI/Architecture/SPBPillarGraphNode.h"

#include "Architecture/PBPillars.h"
#include "SGraphNodeAI.h"
#include "SGraphPanel.h"
#include "Styling/AppStyle.h"
#include "UI/Architecture/PBPillarGraph.h"
#include "UI/Architecture/SPBPillarTreeEditor.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Text/STextBlock.h"

namespace ProtoBoyPillarGraphNode
{
	/** Body colors per type, borrowed from the Behavior Tree editor's palette. */
	const FLinearColor CoreColor(0.24f, 0.055f, 0.715f);
	const FLinearColor FeatureColor(0.42f, 0.17f, 0.02f);
	const FLinearColor ContentColor(0.0f, 0.28f, 0.35f);
	const FLinearColor InterfaceColor(0.38f, 0.04f, 0.12f);
	const FLinearColor AssetColor(0.0f, 0.07f, 0.4f);
	const FLinearColor ToolColor(0.0f, 0.4f, 0.22f);
	const FLinearColor NoTypeColor(0.1f, 0.1f, 0.1f);
	const FLinearColor RootColor(0.15f, 0.15f, 0.15f);
	const FLinearColor BorderColor(0.08f, 0.08f, 0.08f);

	const FProtoBoyPillar* FindPillar(const UProtoBoyPillarGraphNode* Node)
	{
		const UProtoBoyPillarGraph* Graph = Node ? Cast<UProtoBoyPillarGraph>(Node->GetGraph()) : nullptr;
		return Graph ? Graph->FindPillar(Node->PillarId) : nullptr;
	}
}

void SProtoBoyPillarGraphNode::Construct(const FArguments& InArgs, UProtoBoyPillarGraphNode* InNode)
{
	SetCursor(EMouseCursor::CardinalCross);
	GraphNode = InNode;
	UpdateGraphNode();

	// A pillar that was just added opens in rename mode (applied by the panel once the node is visible and selected).
	const TSharedPtr<SProtoBoyPillarTreeEditor> Editor = InNode->GetEditor();
	if (Editor.IsValid() && Editor->ConsumePendingRename(InNode->PillarId))
	{
		RequestRename();
	}
}

UProtoBoyPillarGraphNode* SProtoBoyPillarGraphNode::GetPillarNode() const
{
	return Cast<UProtoBoyPillarGraphNode>(GraphNode);
}

void SProtoBoyPillarGraphNode::UpdateGraphNode()
{
	InputPins.Empty();
	OutputPins.Empty();
	RightNodeBox.Reset();
	LeftNodeBox.Reset();

	const bool bRoot = GetPillarNode() && GetPillarNode()->IsRoot();

	GetOrAddSlot(ENodeZone::Center)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(NodeWidth)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("Graph.StateNode.Body"))
				.BorderBackgroundColor(this, &SProtoBoyPillarGraphNode::GetBorderColor)
				.ColorAndOpacity(this, &SProtoBoyPillarGraphNode::GetOpacity)
				.Padding(0.f)
				[
					SNew(SVerticalBox)

					// Input bar (parent)
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
						.BorderBackgroundColor(this, &SProtoBoyPillarGraphNode::GetBodyColor)
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
									.Image(this, &SProtoBoyPillarGraphNode::GetIcon)
									.DesiredSizeOverride(FVector2D(bRoot ? 20.f : 16.f))
								]

								+ SHorizontalBox::Slot()
								.FillWidth(1.f)
								.VAlign(VAlign_Center)
								[
									SAssignNew(InlineEditableText, SInlineEditableTextBlock)
									.Style(FAppStyle::Get(), "Graph.StateNode.NodeTitleInlineEditableText")
									.Text_Lambda([this]() { return GraphNode ? GraphNode->GetNodeTitle(ENodeTitleType::FullTitle) : FText::GetEmpty(); })
									.OnVerifyTextChanged(this, &SProtoBoyPillarGraphNode::OnVerifyNameTextChanged)
									.OnTextCommitted(this, &SProtoBoyPillarGraphNode::OnNameTextCommited)
									.IsReadOnly(this, &SProtoBoyPillarGraphNode::IsNameReadOnly)
									.IsSelected(this, &SProtoBoyPillarGraphNode::IsSelectedExclusively)
									.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
								]
							]

							// Type
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.f, 2.f, 0.f, 0.f)
							[
								SNew(STextBlock)
								.Text(this, &SProtoBoyPillarGraphNode::GetType)
								.Visibility_Lambda([this]() { return GetType().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
								.TextStyle(FAppStyle::Get(), "SmallText")
								.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.6f))
							]

							// Description
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.f, 4.f, 0.f, 0.f)
							[
								SNew(STextBlock)
								.Text(this, &SProtoBoyPillarGraphNode::GetDescription)
								.Visibility_Lambda([this]()
								{
									const TSharedPtr<SGraphPanel> Panel = GetOwnerPanel();
									const bool bDetailed = !Panel.IsValid() || Panel->GetCurrentLOD() > EGraphRenderingLOD::LowDetail;
									return bDetailed && !GetDescription().IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
								})
								.AutoWrapText(true)
								.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.85f))
							]
						]
					]

					// Output bar (children)
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

void SProtoBoyPillarGraphNode::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
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

TSharedPtr<SGraphPin> SProtoBoyPillarGraphNode::CreatePinWidget(UEdGraphPin* Pin) const
{
	// The Behavior Tree pin: a full-width bar that lights up on hover.
	return SNew(SGraphPinAI, Pin);
}

FSlateColor SProtoBoyPillarGraphNode::GetBorderColor() const
{
	return ProtoBoyPillarGraphNode::BorderColor;
}

FSlateColor SProtoBoyPillarGraphNode::GetBodyColor() const
{
	const UProtoBoyPillarGraphNode* Node = GetPillarNode();
	if (!Node || Node->IsRoot())
	{
		return ProtoBoyPillarGraphNode::RootColor;
	}
	const FProtoBoyPillar* Pillar = ProtoBoyPillarGraphNode::FindPillar(Node);
	// One color per type, so the tree's structure reads at a glance.
	switch (Pillar ? Pillar->Type : EProtoBoyPillarType::None)
	{
	case EProtoBoyPillarType::Core: return ProtoBoyPillarGraphNode::CoreColor;
	case EProtoBoyPillarType::Feature: return ProtoBoyPillarGraphNode::FeatureColor;
	case EProtoBoyPillarType::Content: return ProtoBoyPillarGraphNode::ContentColor;
	case EProtoBoyPillarType::Interface: return ProtoBoyPillarGraphNode::InterfaceColor;
	case EProtoBoyPillarType::Asset: return ProtoBoyPillarGraphNode::AssetColor;
	case EProtoBoyPillarType::Tool: return ProtoBoyPillarGraphNode::ToolColor;
	case EProtoBoyPillarType::Game: return ProtoBoyPillarGraphNode::RootColor;
	default: return ProtoBoyPillarGraphNode::NoTypeColor;
	}
}

FLinearColor SProtoBoyPillarGraphNode::GetOpacity() const
{
	const UProtoBoyPillarGraphNode* Node = GetPillarNode();
	const TSharedPtr<SProtoBoyPillarTreeEditor> Editor = Node ? Node->GetEditor() : nullptr;
	const bool bMatches = !Editor.IsValid() || Node->IsRoot() || Editor->MatchesFilter(Node->PillarId);
	return FLinearColor(1.f, 1.f, 1.f, bMatches ? 1.f : 0.3f);
}

const FSlateBrush* SProtoBoyPillarGraphNode::GetIcon() const
{
	const UProtoBoyPillarGraphNode* Node = GetPillarNode();
	if (!Node || Node->IsRoot())
	{
		return FAppStyle::GetBrush("BTEditor.Graph.BTNode.Root.Icon");
	}
	const UEdGraphPin* ChildrenPin = Node->GetOutputPin();
	return FAppStyle::GetBrush(ChildrenPin && !ChildrenPin->LinkedTo.IsEmpty() ? "Icons.FolderOpen" : "Icons.FolderClosed");
}

FText SProtoBoyPillarGraphNode::GetType() const
{
	const UProtoBoyPillarGraphNode* Node = GetPillarNode();
	const FProtoBoyPillar* Pillar = ProtoBoyPillarGraphNode::FindPillar(Node);
	return Pillar ? GetPillarTypeText(Pillar->Type) : FText::GetEmpty();
}

FText SProtoBoyPillarGraphNode::GetDescription() const
{
	const UProtoBoyPillarGraphNode* Node = GetPillarNode();
	const FProtoBoyPillar* Pillar = ProtoBoyPillarGraphNode::FindPillar(Node);
	return Pillar ? FText::FromString(Pillar->Description) : FText::GetEmpty();
}
