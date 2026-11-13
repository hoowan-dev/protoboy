#pragma once

#include "CoreMinimal.h"
#include "Docs/PBDocLayers.h"
#include "Docs/PBDocsProvider.h"

class FProtoBoyDocGenerator;

/** One doc on the map, read from its frontmatter (see ProtoBoyDocLayers for the keys). */
struct FProtoBoyDocNode
{
	/** Stable id from the frontmatter; derived from the path when the doc has a layer but no id. */
	FString Id;
	/** Relative to the docs root, '/' separators. */
	FString Path;
	FString Title;
	EProtoBoyDocLayer Layer = EProtoBoyDocLayer::Design;
	/** The parent's id as written in the frontmatter. May be empty or stale: use FProtoBoyDocMap::GetParent. */
	FString ParentId;
	/** Tech docs: the Design doc's id. */
	FString ImplementsId;
	/** The first line someone actually wrote below the title (hints and quotes skipped), for previews. */
	FString Summary;
	/** The layer came from where the doc is (an ADR in the ADR folder), not from frontmatter. */
	bool bInferred = false;
};

/** One "### Pillar" under the Vision's "## Design Pillars". */
struct FProtoBoyDesignPillar
{
	FString Name;
	FString Text;
};

/** The Vision doc's sections, with hints removed (empty = not written yet). */
struct FProtoBoyVision
{
	bool bExists = false;
	FString Path;
	FString Pitch;
	FString PlayerFantasy;
	TArray<FProtoBoyDesignPillar> Pillars;
	FString NonGoals;
};

/**
 * The documentation map: every doc with a layer in its frontmatter, as a tree. The docs are the source of truth:
 * the map is rebuilt by reading them (Refresh), so edits made in Obsidian show up after a refresh, and everything
 * ProtoBoy changes is written into the docs themselves (frontmatter, new files, the Overview's managed Areas block).
 *
 * Tree: the Vision is the root. A doc's parent is its "protoboy-parent" when that names another doc (and wouldn't
 * make a loop), otherwise the layer default: the Overview sits under the Vision, a Tech doc under the Design doc it
 * implements, everything else under the Overview. Nothing restricts which layer goes under which.
 */
class PROTOBOY_API FProtoBoyDocMap
{
public:
	explicit FProtoBoyDocMap(FProtoBoyDocGenerator& InDocs);

	/** Re-read every doc. Fires OnChanged. */
	void Refresh();

	const TArray<FProtoBoyDocNode>& GetNodes() const;
	/** Docs in the docs folder without a layer (not on the map), e.g. notes written before ProtoBoy. */
	const TArray<FProtoBoyDocInfo>& GetUnmappedDocs() const;

	const FProtoBoyDocNode* Find(const FString& Id) const;
	const FProtoBoyDocNode* FindByPath(const FString& Path) const;
	/** The first Vision / Overview doc (by path), or null. */
	const FProtoBoyDocNode* GetVision() const;
	const FProtoBoyDocNode* GetOverview() const;
	bool HasVisionAndOverview() const { return GetVision() && GetOverview(); }

	/** The doc this one sits under (see the class comment). Null for the Vision, and when there's no Vision. */
	const FProtoBoyDocNode* GetParent(const FString& Id) const;
	/** The parent a doc gets when its frontmatter names none. */
	const FProtoBoyDocNode* GetDefaultParent(const FProtoBoyDocNode& Node) const;
	/** Docs directly under Id, in layer order, then by title. */
	TArray<const FProtoBoyDocNode*> GetChildren(const FString& Id) const;
	/** True if Id is Ancestor or sits anywhere below it. */
	bool IsInSubtree(const FString& Id, const FString& Ancestor) const;
	/** The Tech doc that implements DesignId, or null. */
	const FProtoBoyDocNode* FindTechFor(const FString& DesignId) const;

	/** Create whichever of the Vision and Overview is missing, from their templates. */
	bool CreateVisionAndOverview(FText& OutError);

	/**
	 * Create a doc of Layer titled Title under ParentId (empty = the layer default), from the layer's template.
	 * Design docs nest in folders (Design/Combat/Weapons/Weapons.md); the others go in the layer's folder.
	 */
	bool CreateDoc(EProtoBoyDocLayer Layer, const FString& Title, const FString& ParentId, FString& OutId, FText& OutError);

	/** Create the Tech doc for a Design doc, next to it, with one section per section of the Design doc. */
	bool CreateTechDoc(const FString& DesignId, FString& OutId, FText& OutError);

	/** Move a doc under another (its frontmatter changes; the file stays where it is). Refuses loops and moving the Vision. */
	bool SetParent(const FString& Id, const FString& NewParentId, FText& OutError);

	/** Put an existing doc on the map by adding ProtoBoy's frontmatter. The doc's text is left alone. */
	bool AddToMap(const FString& Path, EProtoBoyDocLayer Layer, FText& OutError);

	/** Rewrite the Overview's managed "areas" block (under "## Areas") with every doc below it. No-op without an Overview. */
	bool UpdateOverviewIndex(FText& OutError);

	FProtoBoyVision ReadVision() const;
	/** The Overview's Summary section, hints removed. */
	FString ReadOverviewSummary() const;

	/** Obsidian wikilink to a doc: [[Design/Combat/Combat|Combat]]. */
	static FString MakeLink(const FProtoBoyDocNode& Node);

	/** Fired after Refresh and after every change ProtoBoy makes. */
	FSimpleMulticastDelegate OnChanged;

private:
	void EnsureLoaded() const;
	void Load() const;
	/** Work out every doc's parent once per load, breaking any loop the frontmatter makes. */
	void ResolveParents() const;
	FString MakeUniqueId(const FString& Title) const;
	/** Write the frontmatter values into a doc, then refresh and update the Overview's index. */
	bool WriteFrontmatter(const FString& Path, const TArray<TPair<FString, FString>>& Values, FText& OutError);
	/** Refresh, update the Overview's index (errors there are logged, not returned) and notify. */
	void AfterWrite();

	FProtoBoyDocGenerator& Docs;
	mutable bool bLoaded = false;
	mutable TArray<FProtoBoyDocNode> Nodes;
	mutable TArray<FProtoBoyDocInfo> Unmapped;
	/** Doc id -> parent id (empty for the root and for docs with no Vision to hang under). */
	mutable TMap<FString, FString> Parents;
};
