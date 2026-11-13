#pragma once

#include "CoreMinimal.h"

/**
 * The layers of the project's documentation, from why the game exists down to how each piece is built:
 *
 *   Game         why the game exists and what it is: pitch, design pillars, non-goals, core loop, and an index of
 *                every other doc. One doc, the top of the map.
 *     Design     what a feature is, from the player's side. Nestable.
 *       Tech     how something is built: a feature's Tech doc implements a Design doc (and mirrors its sections);
 *                shared tech (save/load, input, networking) stands on its own.
 *     Content    what's in the game (levels, items, characters) and how it looks, sounds and reads (style guides).
 *
 * Every doc ends with a Decisions section: dated entries for what was decided there, why, and what it rules out.
 * A doc's layer, id and links live in its frontmatter (see the keys below), so the docs themselves are the model:
 * FProtoBoyDocMap builds the map by reading them. Nothing restricts which layer sits under which.
 */
enum class EProtoBoyDocLayer : uint8
{
	Game,
	Design,
	Tech,
	Content,
};

/** Which template a new doc starts from. Each belongs to one layer (ProtoBoyDocLayers::GetLayer). */
enum class EProtoBoyDocKind : uint8
{
	Game,
	Design,
	/** Implements a Design doc: one section per section of it. Made with Add Tech Doc. */
	FeatureTech,
	/** Shared tech with no Design doc of its own. */
	SharedTech,
	/** What exists: levels, items, quests, characters, balance tables. */
	Content,
	/** How it's presented: art, audio, UX, writing style. A Content doc. */
	StyleGuide,
};

namespace ProtoBoyDocLayers
{
	/** Frontmatter keys ProtoBoy owns. Prefixed so they don't clash with the team's own properties. */
	PROTOBOY_API extern const TCHAR* IdKey;
	PROTOBOY_API extern const TCHAR* LayerKey;
	PROTOBOY_API extern const TCHAR* ParentKey;
	PROTOBOY_API extern const TCHAR* ImplementsKey;

	/** Every layer, top to bottom. */
	PROTOBOY_API TArray<EProtoBoyDocLayer> GetAllLayers();

	/** The frontmatter value, e.g. "design". */
	PROTOBOY_API FString ToKey(EProtoBoyDocLayer Layer);
	/**
	 * Parse a frontmatter value (any case). Keys from the eight-layer version still work: vision and overview are
	 * Game, systems is Tech, direction is Content. False for anything else, including "decision" (see IsRetiredKey).
	 */
	PROTOBOY_API bool FromKey(const FString& Key, EProtoBoyDocLayer& OutLayer);
	/** Layers that no longer exist and whose docs stay off the map: "decision" (ADRs). */
	PROTOBOY_API bool IsRetiredKey(const FString& Key);

	PROTOBOY_API FText GetDisplayName(EProtoBoyDocLayer Layer);
	/** One line on what docs of this layer are for (tooltips). */
	PROTOBOY_API FText GetDescription(EProtoBoyDocLayer Layer);

	PROTOBOY_API EProtoBoyDocLayer GetLayer(EProtoBoyDocKind Kind);
	PROTOBOY_API FText GetKindName(EProtoBoyDocKind Kind);
	PROTOBOY_API FText GetKindDescription(EProtoBoyDocKind Kind);
	/** Kinds added from the map's Add menu. The Game doc is one per project; feature Tech docs come from Add Tech Doc. */
	PROTOBOY_API TArray<EProtoBoyDocKind> GetAddableKinds();
	/** Template files name kinds as "design", "tech" (shared), "content" and "style". */
	PROTOBOY_API bool FromTemplateKey(const FString& Key, EProtoBoyDocKind& OutKind);

	/** Folder new docs of this kind go in, relative to the docs root. Empty for the Game doc; feature Tech docs sit next to their Design doc. */
	PROTOBOY_API FString GetFolder(EProtoBoyDocKind Kind);

	struct FHeading
	{
		FString Title;
		/** What goes under it. Written as an HTML comment, so it's hidden when reading and ignored by ProtoBoy. */
		FString Hint;
	};

	/** The heading every doc ends with. */
	PROTOBOY_API const TCHAR* GetDecisionsHeadingTitle();
	PROTOBOY_API FHeading GetDecisionsHeading();

	/** The "## " sections a new doc of this kind starts with, Decisions last. FeatureTech mirrors its Design doc (FProtoBoyDocMap). */
	PROTOBOY_API TArray<FHeading> GetHeadings(EProtoBoyDocKind Kind);

	/** The hint under each mirrored section of a feature Tech doc. */
	PROTOBOY_API FString GetTechSectionHint();

	/** The intro line under a new doc's title. */
	PROTOBOY_API FString GetIntro(EProtoBoyDocKind Kind);

	/**
	 * A new doc's text after its frontmatter: "# Title", an intro quote, an optional Lead paragraph (what the doc covers,
	 * e.g. from a template), then each heading with its hint.
	 */
	PROTOBOY_API FString MakeBody(const FString& Title, const FString& Intro, const TArray<FHeading>& Headings, const FString& Lead = FString());
}
