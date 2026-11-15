#pragma once

#include "CoreMinimal.h"

/**
 * Pillar 3 (asset sourcing): where agents should get assets from, so they don't default to AI-generated ones.
 * Team-wide choices live in UProtoBoySettings (Asset Sourcing). Each source gets a subfolder (Sourcing/Fab/, ...).
 */
namespace ProtoBoyAssetSourcing
{
	/** Markdown for agents: the sourcing policy, enabled sources, and where the style assets must match is written (Content docs). */
	PROTOBOY_API FString DescribePolicyForAgents();
}
