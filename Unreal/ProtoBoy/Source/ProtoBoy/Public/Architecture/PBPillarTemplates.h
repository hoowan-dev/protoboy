#pragma once

#include "CoreMinimal.h"
#include "Architecture/PBPillars.h"
#include "PBPillarTemplates.generated.h"

/**
 * A preset pillar tree for a kind of game (genre and scope), offered by Setup Pillars from Template > Build From
 * Template. Stored as one JSON file per template in the plugin's Resources/PillarTemplates/ folder, serialized the
 * same way as the pillar file (FJsonObjectConverter). Pillars use the pillar file's shape: exactly one root (bRoot,
 * the game; its Name is replaced with the project's name on load), and every other pillar points at its parent by Id.
 */
USTRUCT()
struct PROTOBOY_API FProtoBoyPillarTemplate
{
	GENERATED_BODY()

	static constexpr int32 CurrentVersion = 1;

	UPROPERTY()
	int32 Version = CurrentVersion;

	/** Stable identifier, e.g. "generic". */
	UPROPERTY()
	FString Id;

	UPROPERTY()
	FString Name;

	UPROPERTY()
	FString Description;

	/** Genre and scope the template is meant for; empty when it fits any. */
	UPROPERTY()
	FString Genre;

	UPROPERTY()
	FString Scope;

	/** Position in the template list (lowest first; ties sorted by name). */
	UPROPERTY()
	int32 SortOrder = 0;

	UPROPERTY()
	TArray<FProtoBoyPillar> Pillars;
};

namespace ProtoBoyPillarTemplates
{
	/** The folder the built-in templates are read from (the plugin's Resources/PillarTemplates). */
	PROTOBOY_API FString GetTemplatesDir();

	/**
	 * Read every template file in GetTemplatesDir(), in display order. Files that can't be read, are from a newer
	 * ProtoBoy, or don't have exactly one root are skipped with a warning in the log.
	 */
	PROTOBOY_API TArray<FProtoBoyPillarTemplate> LoadTemplates();
}
