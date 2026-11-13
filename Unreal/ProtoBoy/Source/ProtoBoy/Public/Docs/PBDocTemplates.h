#pragma once

#include "CoreMinimal.h"
#include "PBDocTemplates.generated.h"

/** A design pillar a template suggests for the Game doc. Placeholders: the team rewrites them for their game. */
USTRUCT()
struct PROTOBOY_API FProtoBoyDocTemplatePillar
{
	GENERATED_BODY()

	UPROPERTY()
	FString Name;

	/** The feeling it gives the player. */
	UPROPERTY()
	FString Feeling;

	/** What it rules out. */
	UPROPERTY()
	FString RulesOut;
};

/** One doc a template creates below the Game doc. */
USTRUCT()
struct PROTOBOY_API FProtoBoyDocTemplateDoc
{
	GENERATED_BODY()

	/** Unique within the template; children point at it with Parent. Not the doc's id on the map. */
	UPROPERTY()
	FString Id;

	UPROPERTY()
	FString Title;

	/** Which template the doc starts from (ProtoBoyDocLayers::FromTemplateKey): design, tech (shared), content or style. */
	UPROPERTY()
	FString Kind;

	/** Id of the template doc this one goes under; empty = the Game doc. Parents come before their children. */
	UPROPERTY()
	FString Parent;

	/** One line on what the doc covers, written as its first paragraph. */
	UPROPERTY()
	FString Summary;
};

/**
 * A starting set of docs for the doc map: design pillars for the Game doc, and a tree of docs below it.
 * One JSON file each in the plugin's Resources/DocTemplates/ (FJsonObjectConverter format). Applying one only creates
 * docs that don't exist yet (FProtoBoyDocMap::ApplyTemplate).
 */
USTRUCT()
struct PROTOBOY_API FProtoBoyDocTemplate
{
	GENERATED_BODY()

	static constexpr int32 CurrentVersion = 1;

	UPROPERTY()
	int32 Version = CurrentVersion;

	UPROPERTY()
	FString Id;

	UPROPERTY()
	FString Name;

	UPROPERTY()
	FString Description;

	/** The kind of game it's for; empty = any genre (the size templates). */
	UPROPERTY()
	FString Genre;

	/** List position in the setup window (lower first). */
	UPROPERTY()
	int32 SortOrder = 0;

	UPROPERTY()
	TArray<FProtoBoyDocTemplatePillar> Pillars;

	UPROPERTY()
	TArray<FProtoBoyDocTemplateDoc> Docs;
};

namespace ProtoBoyDocTemplates
{
	/** The plugin's Resources/DocTemplates folder. */
	PROTOBOY_API FString GetTemplatesDir();

	/** Every valid template, by SortOrder then name. Bad files are skipped with a warning. */
	PROTOBOY_API TArray<FProtoBoyDocTemplate> LoadTemplates();
}
