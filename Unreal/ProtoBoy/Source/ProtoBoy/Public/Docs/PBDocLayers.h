#pragma once

#include "CoreMinimal.h"

/**
 * The layers of the project's documentation, from why the game exists down to how each piece is built:
 *
 *   Vision            why the game exists: pitch, design pillars (3-5 experience goals), non-goals. One doc.
 *     Overview        what the game is, as a whole: the master index. One doc.
 *       Design        what each feature is, from the player's side. Nestable.
 *         Tech        how a feature is built. Pairs with its Design doc and mirrors its sections.
 *       Systems       shared tech with no player face (save/load, input, networking).
 *       Content       what exists in the game (levels, items, quests, characters).
 *       Direction     how it looks, sounds and reads (art, audio, UX, writing style).
 *       Decision      why things are the way they are (ADRs).
 *
 * A doc's layer, id and links live in its frontmatter (see the keys below), so the docs themselves are the model:
 * FProtoBoyDocMap builds the map by reading them. Nothing restricts which layer sits under which.
 */
enum class EProtoBoyDocLayer : uint8
{
	Vision,
	Overview,
	Design,
	Tech,
	Systems,
	Content,
	Direction,
	Decision,
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
	/** Layers added from the map's Add menu: Vision and Overview are one each, Tech comes from Add Tech Doc, Decisions are ADRs. */
	PROTOBOY_API TArray<EProtoBoyDocLayer> GetAddableLayers();

	/** The frontmatter value, e.g. "design". */
	PROTOBOY_API FString ToKey(EProtoBoyDocLayer Layer);
	/** Parse a frontmatter value (any case). */
	PROTOBOY_API bool FromKey(const FString& Key, EProtoBoyDocLayer& OutLayer);

	PROTOBOY_API FText GetDisplayName(EProtoBoyDocLayer Layer);
	/** One line on what docs of this layer are for (tooltips). */
	PROTOBOY_API FText GetDescription(EProtoBoyDocLayer Layer);

	/** Folder new docs of this layer go in, relative to the docs root. Empty for Vision and Overview (the root itself). */
	PROTOBOY_API FString GetFolder(EProtoBoyDocLayer Layer);

	struct FHeading
	{
		FString Title;
		/** What goes under it. Written as an HTML comment, so it's hidden when reading and ignored by ProtoBoy. */
		FString Hint;
	};

	/** The "## " sections a new doc of this layer starts with. Tech docs mirror their Design doc instead (see FProtoBoyDocMap). */
	PROTOBOY_API TArray<FHeading> GetHeadings(EProtoBoyDocLayer Layer);

	/** The hint under each section of a Tech doc. */
	PROTOBOY_API FString GetTechSectionHint();

	/** A new doc's text after its frontmatter: "# Title", an intro quote, then each heading with its hint. */
	PROTOBOY_API FString MakeBody(const FString& Title, const FString& Intro, const TArray<FHeading>& Headings);

	/** The intro line under a new doc's title. */
	PROTOBOY_API FString GetIntro(EProtoBoyDocLayer Layer);
}
