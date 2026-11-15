#pragma once

#include "CoreMinimal.h"

/**
 * The layers of the project's documentation:
 *
 *   Game       the game as a whole: pitch, design pillars, non-goals, core loop, and an index of every other doc. One
 *              doc, named after the project, at the top of the map.
 *   Design     what a player should be able to do or see.
 *   Tech       the technical details of how something works.
 *   Content    the assets needed to make it happen, and the style they must have. Agents read the Content doc for an
 *              asset before looking for it on storefronts (Fab), so it says what to search for and what to avoid.
 *
 * Every doc ends with a Decisions section: dated entries for what was decided there, why, and what it rules out.
 * A doc's layer, id and parent live in its frontmatter (see the keys below), so the docs themselves are the model:
 * FProtoBoyDocMap builds the map by reading them. The Game doc is always the root; every other doc can go under any
 * doc the team likes (a Tech doc under a Design doc, Content under Tech, a Design doc under Content, ...).
 */
enum class EProtoBoyDocLayer : uint8
{
	Game,
	Design,
	Tech,
	Content,
};

/** Which template a new doc starts from. One per layer (ProtoBoyDocLayers::GetLayer). */
enum class EProtoBoyDocKind : uint8
{
	Game,
	Design,
	Tech,
	Content,
};

namespace ProtoBoyDocLayers
{
	/** Frontmatter keys ProtoBoy owns. Prefixed so they don't clash with the team's own properties. */
	PROTOBOY_API extern const TCHAR* IdKey;
	PROTOBOY_API extern const TCHAR* LayerKey;
	PROTOBOY_API extern const TCHAR* ParentKey;
	/** Older versions linked a feature's Tech doc to its Design doc with this. Read as the parent when there's no parent key. */
	PROTOBOY_API extern const TCHAR* LegacyImplementsKey;

	/** Every layer, top to bottom. */
	PROTOBOY_API TArray<EProtoBoyDocLayer> GetAllLayers();

	/** The frontmatter value, e.g. "design". */
	PROTOBOY_API FString ToKey(EProtoBoyDocLayer Layer);
	/**
	 * Parse a frontmatter value (any case). Keys from older versions still work: vision and overview are Game, systems
	 * is Tech, direction and style are Content. False for anything else, including "decision" (see IsRetiredKey).
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
	PROTOBOY_API EProtoBoyDocKind GetKind(EProtoBoyDocLayer Layer);
	/** Kinds added from the map's Add menu: Design, Tech and Content (the Game doc is one per project). */
	PROTOBOY_API TArray<EProtoBoyDocKind> GetAddableKinds();
	/** Template files name kinds as "design", "tech" and "content". */
	PROTOBOY_API bool FromTemplateKey(const FString& Key, EProtoBoyDocKind& OutKind);

	/** Folder new docs of this kind go in when they sit under the Game doc, relative to the docs root. Empty for the Game doc (docs root). */
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

	/** The "## " sections a new doc of this kind starts with, Decisions last. */
	PROTOBOY_API TArray<FHeading> GetHeadings(EProtoBoyDocKind Kind);

	/** The intro line under a new doc's title. */
	PROTOBOY_API FString GetIntro(EProtoBoyDocKind Kind);

	/**
	 * A new doc's text after its frontmatter: "# Title", an intro quote, an optional Lead paragraph (what the doc covers,
	 * e.g. from a template), then each heading with its hint, followed by its entry in SectionText (heading -> Markdown,
	 * matched ignoring case), if any.
	 */
	PROTOBOY_API FString MakeBody(const FString& Title, const FString& Intro, const TArray<FHeading>& Headings, const FString& Lead = FString(),
		const TArray<TPair<FString, FString>>& SectionText = {});
}
