#include "UI/Planning/SPBPillarGraphNode.h"

#include "PBSubsystem.h"
#include "Planning/PBPillars.h"
#include "SGraphNodeAI.h"
#include "SGraphPanel.h"
#include "Styling/AppStyle.h"
#include "UI/Planning/PBPillarGraph.h"
#include "UI/Planning/SPBPillarTreeEditor.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Text/STextBlock.h"

namespace ProtoBoyPillarGraphNode
{
	/** Body colors, borrowed from the Behavior Tree editor's palette (task, decorator, service, ...). */
	const FLinearColor KindColors[] = {
		FLinearColor(0.24f, 0.055f, 0.715f),
		FLinearColor(0.0f, 0.07f, 0.4f),
		FLinearColor(0.0f, 0.4f, 0.22f),
		FLinearColor(0.0f, 0.28f, 0.35f),
		FLinearColor(0.42f, 0.17f, 0.02f),
		FLinearColor(0.38f, 0.04f, 0.12f),
	};
	const FLinearColor NoKindColor(0.1f, 0.1f, 0.1f);
	const FLinearColor RootColor(0.15f, 0.15f, 0.15f);
	const FLinearColor BorderColor(0.08f, 0.08f, 0.08f);

	const FProtoBoyPillar* FindPillar(const FString& Id)
	{
		const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
		return Subsystem && !Id.IsEmpty() ? Subsystem->GetPillars().Find(Id) : nullptr;
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

							// Kind
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.f, 2.f, 0.f, 0.f)
							[
								SNew(STextBlock)
								.Text(this, &SProtoBoyPillarGraphNode::GetKind)
								.Visibility_Lambda([this]() { return GetKind().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
								.TextStyle(FAppStyle::Get(), "SmallText")
								.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.6f))
							]

							// Vision
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.f, 4.f, 0.f, 0.f)
							[
								SNew(STextBlock)
								.Text(this, &SProtoBoyPillarGraphNode::GetVision)
								.Visibility_Lambda([this]()
								{
									const TSharedPtr<SGraphPanel> Panel = GetOwnerPanel();
									const bool bDetailed = !Panel.IsValid() || Panel->GetCurrentLOD() > EGraphRenderingLOD::LowDetail;
									return bDetailed && !GetVision().IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
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
	const FProtoBoyPillar* Pillar = ProtoBoyPillarGraphNode::FindPillar(Node->PillarId);
	const FString Kind = Pillar ? Pillar->Kind.ToLower() : FString();
	if (Kind.IsEmpty())
	{
		return ProtoBoyPillarGraphNode::NoKindColor;
	}
	// Same kind, same color, so the team's categories read at a glance.
	return ProtoBoyPillarGraphNode::KindColors[GetTypeHash(Kind) % UE_ARRAY_COUNT(ProtoBoyPillarGraphNode::KindColors)];
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

FText SProtoBoyPillarGraphNode::GetKind() const
{
	const UProtoBoyPillarGraphNode* Node = GetPillarNode();
	if (Node && Node->IsRoot())
	{
		return NSLOCTEXT("SProtoBoyPillarGraphNode", "RootKind", "Game");
	}
	const FProtoBoyPillar* Pillar = Node ? ProtoBoyPillarGraphNode::FindPillar(Node->PillarId) : nullptr;
	return Pillar ? FText::FromString(Pillar->Kind) : FText::GetEmpty();
}

FText SProtoBoyPillarGraphNode::GetVision() const
{
	const UProtoBoyPillarGraphNode* Node = GetPillarNode();
	const FProtoBoyPillar* Pillar = Node ? ProtoBoyPillarGraphNode::FindPillar(Node->PillarId) : nullptr;
	return Pillar ? FText::FromString(Pillar->Vision) : FText::GetEmpty();
}
