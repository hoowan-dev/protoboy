#pragma once

#include "CoreMinimal.h"

/**
 * Pillar 3 (asset sourcing): where agents should get assets from, so they don't default to AI-generated ones.
 * Team-wide choices live in UProtoBoySettings (Asset Sourcing). Each source gets a subfolder (Sourcing/Fab/, ...).
 */
/** Kinds of asset a search can be limited to. Each source maps them to its own filters. */
enum class EProtoBoyAssetType : uint8
{
	Model3D,

	MaterialTexture,
	SpriteFlipbook,
	Decal,
	Brush,
	HDRI,

	Animation,
	Audio,
	VFX,
	UI,

	GameSystem,
	GameTemplate,
	ToolPlugin,
	Tutorial,

	Count
};

/** A search for assets, independent of the source. Each enabled source turns it into its own search. */
struct FProtoBoyAssetSearch
{
	FString Query;
	/** Empty: every type. */
	TArray<EProtoBoyAssetType> Types;
	bool bFreeOnly = false;
	/** Price range in USD; unset is no limit. Ignored with bFreeOnly. */
	TOptional<float> MinPrice;
	TOptional<float> MaxPrice;
	/** Refine the query from the ProtoBoy docs (Content docs' style sections, and the images in the docs). */
	bool bUseDocsContext = true;
	/** Refine the query from the assets already in the project (/Game), to match them and avoid duplicates. */
	bool bUseProjectAssetsContext = true;
};

/** An asset source ProtoBoy knows about (Fab, ...), whether or not the team enabled it. */
struct FProtoBoyAssetSource
{
	FText Name;
	FText Description;
	/** Brush name in FProtoBoyStyle. */
	FName Icon;
	/** Enabled for this project (its UProtoBoySettings toggle). */
	TFunction<bool()> IsEnabled;
	/** Open the search in the user's browser. False with OutError when it couldn't. */
	TFunction<bool(const FProtoBoyAssetSearch& Search, FString& OutUrl, FText& OutError)> OpenSearch;
};

namespace ProtoBoyAssetSourcing
{
	/** Every source ProtoBoy supports, in display order. Add new sources here. */
	PROTOBOY_API const TArray<FProtoBoyAssetSource>& GetSources();

	/** At least one asset source is enabled for this project. */
	PROTOBOY_API bool HasEnabledSource();

	/** Display name of an asset type ("3D Models", ...). */
	PROTOBOY_API FText GetAssetTypeName(EProtoBoyAssetType Type);

	/**
	 * Open Search on every enabled source. Returns how many opened; OutErrors gets one line per source that failed
	 * ("Fab: ...").
	 */
	PROTOBOY_API int32 OpenSearch(const FProtoBoyAssetSearch& Search, TArray<FText>& OutErrors);

	/**
	 * What the Asset Sourcing page waits on beyond the dashboard's general setup: preferring sourced assets over
	 * AI-generated ones, and at least one enabled source. Empty when both are set.
	 */
	PROTOBOY_API TArray<FText> GetMissingSetup();

	/** Markdown for agents: the sourcing policy, enabled sources, and where the style assets must match is written (Content docs). */
	PROTOBOY_API FString DescribePolicyForAgents();
}
