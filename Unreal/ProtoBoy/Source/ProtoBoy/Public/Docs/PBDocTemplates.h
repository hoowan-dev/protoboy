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

/** Example text for one "## " section of a doc, written when a template is applied with example documentation. */
USTRUCT()
struct PROTOBOY_API FProtoBoyDocTemplateSection
{
	GENERATED_BODY()

	/** The section's heading, as the doc's kind names it (e.g. "The Player Can"). Sections the kind doesn't have are skipped. */
	UPROPERTY()
	FString Heading;

	/** Markdown written under the heading's hint. */
	UPROPERTY()
	FString Text;
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

	/** Which template the doc starts from (ProtoBoyDocLayers::FromTemplateKey): design, tech or content. */
	UPROPERTY()
	FString Kind;

	/** Id of the template doc this one goes under (any kind); empty = the Game doc. Parents come before their children. */
	UPROPERTY()
	FString Parent;

	/** One line on what the doc covers, written as its first paragraph. */
	UPROPERTY()
	FString Summary;

	/** Example content for its sections, written only when the team asks for example documentation. */
	UPROPERTY()
	TArray<FProtoBoyDocTemplateSection> Examples;
};

/**
 * A starting set of docs for the doc map: design pillars for the Game doc, and a tree of Design, Tech and Content docs
 * below it.
 * One JSON file each in the plugin's Resources/DocTemplates/ (FJsonObjectConverter format). Applying one replaces
 * the docs on the map with its own (FProtoBoyDocMap::ApplyTemplate).
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

	/** Drafted by an agent from the project (FProtoBoyDocGenerationRun), not a built-in preset: changes the notes written into the docs. */
	UPROPERTY()
	bool bGenerated = false;

	UPROPERTY()
	TArray<FProtoBoyDocTemplatePillar> Pillars;

	/** Example content for the Game doc's sections (pitch, core loop, ...), written only with example documentation. */
	UPROPERTY()
	TArray<FProtoBoyDocTemplateSection> GameExamples;

	UPROPERTY()
	TArray<FProtoBoyDocTemplateDoc> Docs;
};

namespace ProtoBoyDocTemplates
{
	/** True if applying the template with example documentation writes anything more than the blank templates. */
	PROTOBOY_API bool HasExamples(const FProtoBoyDocTemplate& Template);

	/** Why a template can't be used (bad kinds, missing or out-of-order parents, duplicate ids), or empty. */
	PROTOBOY_API FString Validate(const FProtoBoyDocTemplate& Template);

	/** The plugin's Resources/DocTemplates folder. */
	PROTOBOY_API FString GetTemplatesDir();

	/** Every valid template, by SortOrder then name. Bad files are skipped with a warning. */
	PROTOBOY_API TArray<FProtoBoyDocTemplate> LoadTemplates();
}
