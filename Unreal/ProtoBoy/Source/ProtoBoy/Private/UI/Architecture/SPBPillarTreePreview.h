#pragma once

#include "UObject/GCObject.h"
#include "Widgets/SCompoundWidget.h"

struct FProtoBoyPillar;
class SGraphEditor;
class UProtoBoyPillarGraph;

/**
 * A read-only pillar tree graph for pillars that aren't in the pillar file, e.g. a template. Looks like the tree
 * editor's graph (same nodes, same top-down layout) but can only be panned and zoomed.
 */
class SProtoBoyPillarTreePreview : public SCompoundWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SProtoBoyPillarTreePreview) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Show these pillars (the root first, the rest pointing at their parent by Id) and zoom to fit them. */
	void SetPillars(const TArray<FProtoBoyPillar>& Pillars);

	// FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("SProtoBoyPillarTreePreview"); }

private:
	TObjectPtr<UProtoBoyPillarGraph> Graph;
	TSharedPtr<SGraphEditor> GraphEditor;
};
