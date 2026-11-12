#pragma once

#include "CoreMinimal.h"

class FProtoBoyDocGenerator;
class FProtoBoyPillarService;

enum class EProtoBoyPillarDocAction : uint8
{
	/** New doc from the pillar template. */
	Create,
	/** The doc's ProtoBoy header (or the Architecture doc's pillar overview) changes. */
	Update,
	/** The pillar was removed: its doc's header is marked archived. Nothing else is touched. */
	Archive,
	UpToDate,
};

/** One planned change, shown in the preview before anything is written. */
struct FProtoBoyPillarDocChange
{
	EProtoBoyPillarDocAction Action = EProtoBoyPillarDocAction::UpToDate;
	/** Empty for the Architecture doc's overview. */
	FString PillarId;
	/** "Gameplay / Combat", or a label for non-pillar docs. */
	FString Label;
	/** Doc path relative to the docs folder. */
	FString DocPath;
	FString Detail;
	/** Create: the whole new doc. */
	FString NewText;
	/** Update / Archive: the managed block to set (re-applied to the doc's current text when the plan is applied). */
	FString BlockId;
	FString BlockHeading;
	FString BlockContent;
};

/**
 * Generates docs from the pillar tree. Plan() computes every change without writing; Apply() writes a plan.
 *
 * Each pillar gets <PillarDocsFolder>/<ancestors>/<Name>.md from a template (Vision, Goals, Non-goals, Design, Rules,
 * Interfaces, Art & assets, Open questions). ProtoBoy only owns a managed "pillar" header block in it (kind, vision,
 * parent/children/dependency links); the rest is the team's. Docs keep their path when a pillar is renamed or moved.
 * The Architecture doc, if it exists, gets a managed "pillars" overview of the tree.
 */
class PROTOBOY_API FProtoBoyPillarDocSync
{
public:
	static TArray<FProtoBoyPillarDocChange> Plan(const FProtoBoyPillarService& Pillars, const FProtoBoyDocGenerator& Docs);

	/** Write every change that isn't UpToDate, recording new doc paths and clearing archived pillars. */
	static bool Apply(const TArray<FProtoBoyPillarDocChange>& Changes, FProtoBoyPillarService& Pillars, FProtoBoyDocGenerator& Docs,
		int32& OutWritten, FText& OutError);

	/** Number of changes that would write something. */
	static int32 CountPending(const TArray<FProtoBoyPillarDocChange>& Changes);
};
