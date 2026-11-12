#pragma once

#include "CoreMinimal.h"

/**
 * Pillar 3 (asset sourcing): where agents should get assets from, so they don't default to AI-generated ones.
 * Team-wide choices live in UProtoBoySettings (Asset Sourcing). Each source gets a subfolder (Sourcing/Fab/, ...).
 */
namespace ProtoBoyAssetSourcing
{
	/** Normalized art direction doc path, or empty if none is set. */
	PROTOBOY_API FString GetArtDirectionDocPath();

	/** Markdown for agents: the sourcing policy, enabled sources and where the art direction lives. */
	PROTOBOY_API FString DescribePolicyForAgents();
}
