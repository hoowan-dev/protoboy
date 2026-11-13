#pragma once

#include "CoreMinimal.h"
#include "Docs/PBDocLayers.h"
#include "Docs/PBDocsProvider.h"

class FProtoBoyDocGenerator;
struct FProtoBoyDocTemplate;
struct FProtoBoyDocTemplatePillar;

/** One doc on the map, read from its frontmatter (see ProtoBoyDocLayers for the keys). */
struct FProtoBoyDocNode
{
	/** Stable id from the frontmatter; derived from the path when the doc has a layer but no id. */
	FString Id;
	/** Relative to the docs root, '/' separators. */
	FString Path;
	FString Title;
	EProtoBoyDocLayer Layer = EProtoBoyDocLayer::Design;
	/** The layer as written ("vision", "systems", ... from the eight-layer version map onto the four layers). */
	FString LayerKey;
	/** The parent's id as written in the frontmatter. May be empty or stale: use FProtoBoyDocMap::GetParent. */
	FString ParentId;
	/** Tech docs: the Design doc they implement. Empty for shared tech. */
	FString ImplementsId;
	/** The first line someone actually wrote below the title (hints and quotes skipped), for previews. */
	FString Summary;
};

/** One "### Pillar" under the Game doc's "## Design Pillars". */
struct FProtoBoyDesignPillar
{
	FString Name;
	FString Text;
};

/** The Game doc's sections, with hints removed (empty = not written yet). */
struct FProtoBoyGameDoc
{
	bool bExists = false;
	FString Path;
	FString Pitch;
	FString PlayerFantasy;
	TArray<FProtoBoyDesignPillar> Pillars;
	FString NonGoals;
	FString CoreLoop;
};

/** What applying a template would do, for the setup window's confirmation step. */
struct FProtoBoyDocTemplatePlan
{
	struct FEntry
	{
		FString Title;
		EProtoBoyDocKind Kind = EProtoBoyDocKind::Design;
		/** Where it would be written. */
		FString Path;
		/** Levels below the Game doc (0 = directly under it). */
		int32 Depth = 0;
		/** A doc is already at Path: it's kept as it is, and not written. */
		bool bExists = false;
	};

	/** There's no Game doc yet, so the template creates it (with its design pillars). */
	bool bCreatesGame = false;
	/** The template's docs, in order. */
	TArray<FEntry> Docs;
	/** Docs that would be written (the ones that don't exist yet), the Game doc included. */
	int32 NumToCreate = 0;
};

/**
 * The documentation map: every doc with a layer in its frontmatter, as a tree. The docs are the source of truth:
 * the map is rebuilt by reading them (Refresh), so edits made in Obsidian show up after a refresh, and everything
 * ProtoBoy changes is written into the docs themselves (frontmatter, new files, the Game doc's managed Areas block).
 *
 * Tree: the Game doc is the root. A doc's parent is its "protoboy-parent" when that names another doc (and wouldn't
 * make a loop), otherwise the layer default: a feature's Tech doc sits under the Design doc it implements, everything
 * else under the Game doc. Nothing restricts which layer goes under which.
 *
 * Docs from the eight-layer version keep working: a Vision and an Overview are both Game docs (the Vision is the root,
 * the Overview hangs under it), Systems docs are shared Tech, Direction docs are Content. ADRs ("decision", or in the
 * ADR folder) stay off the map: decisions now live in each doc's Decisions section (RecordDecision).
 */
class PROTOBOY_API FProtoBoyDocMap
{
public:
	explicit FProtoBoyDocMap(FProtoBoyDocGenerator& InDocs);

	/** Re-read every doc. Fires OnChanged. */
	void Refresh();

	const TArray<FProtoBoyDocNode>& GetNodes() const;
	/** Docs in the docs folder without a layer (not on the map), e.g. notes written before ProtoBoy. ADRs aren't listed. */
	const TArray<FProtoBoyDocInfo>& GetUnmappedDocs() const;

	const FProtoBoyDocNode* Find(const FString& Id) const;
	const FProtoBoyDocNode* FindByPath(const FString& Path) const;
	/** The Game doc: the map's root. With several, the first by path, preferring one that wasn't an old Overview. */
	const FProtoBoyDocNode* GetGame() const;
	bool HasGameDoc() const { return GetGame() != nullptr; }

	/** The doc this one sits under (see the class comment). Null for the Game doc, and when there's no Game doc. */
	const FProtoBoyDocNode* GetParent(const FString& Id) const;
	/** The parent a doc gets when its frontmatter names none. */
	const FProtoBoyDocNode* GetDefaultParent(const FProtoBoyDocNode& Node) const;
	/** Docs directly under Id, in layer order, then by title. */
	TArray<const FProtoBoyDocNode*> GetChildren(const FString& Id) const;
	/** True if Id is Ancestor or sits anywhere below it. */
	bool IsInSubtree(const FString& Id, const FString& Ancestor) const;
	/** The Tech doc that implements DesignId, or null. */
	const FProtoBoyDocNode* FindTechFor(const FString& DesignId) const;

	/** Create the Game doc (Game.md) from its template, if there isn't one. */
	bool CreateGameDoc(FText& OutError);

	/**
	 * Create a doc of Kind (one of ProtoBoyDocLayers::GetAddableKinds) titled Title under ParentId (empty = the Game
	 * doc), from its template. Design docs nest in folders (Design/Combat/Weapons/Weapons.md); shared Tech goes in
	 * Tech/, Content and style guides in Content/.
	 */
	bool CreateDoc(EProtoBoyDocKind Kind, const FString& Title, const FString& ParentId, FString& OutId, FText& OutError);

	/** Create the Tech doc for a Design doc, next to it, with one section per section of the Design doc. */
	bool CreateTechDoc(const FString& DesignId, FString& OutId, FText& OutError);

	/** Move a doc under another (its frontmatter changes; the file stays where it is). Refuses loops and moving the Game doc. */
	bool SetParent(const FString& Id, const FString& NewParentId, FText& OutError);

	/** Put an existing doc on the map by adding ProtoBoy's frontmatter. The doc's text is left alone. */
	bool AddToMap(const FString& Path, EProtoBoyDocLayer Layer, FText& OutError);

	/**
	 * Append a dated entry to a doc's Decisions section (created at the end if missing): what was decided, why, and what
	 * it rules out. By names who recorded it (e.g. "Claude Code via ProtoBoy").
	 */
	bool RecordDecision(const FString& Id, const FString& Decision, const FString& Why, const FString& RulesOut, const FString& By, FText& OutError);

	/** What ApplyTemplate would create and what it would leave alone. */
	FProtoBoyDocTemplatePlan PlanTemplate(const FProtoBoyDocTemplate& Template) const;

	/**
	 * Create a template's docs: the Game doc (with the template's design pillars) if missing, then each of its docs that
	 * doesn't exist yet, in the same places CreateDoc would put them. Never overwrites a doc: existing ones are kept, and
	 * the template's docs below them still go under them. OutCreated counts the docs written.
	 */
	bool ApplyTemplate(const FProtoBoyDocTemplate& Template, int32& OutCreated, FText& OutError);

	/** Rewrite the Game doc's managed "areas" block (under "## Areas") with every doc below it. No-op without a Game doc. */
	bool UpdateAreasIndex(FText& OutError);

	FProtoBoyGameDoc ReadGame() const;

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
	/** Write the frontmatter values into a doc, then refresh and update the Areas index. */
	bool WriteFrontmatter(const FString& Path, const TArray<TPair<FString, FString>>& Values, FText& OutError);
	/** Refresh, update the Areas index (errors there are logged, not returned) and notify. */
	void AfterWrite();
	/** Write Game.md if there's no Game doc, without refreshing. Pillars (from a template) replace the example pillar. */
	bool WriteMissingGameDoc(const TArray<FProtoBoyDocTemplatePillar>* Pillars, const FString& TemplateName, bool& bOutWrote, FText& OutError);

	FProtoBoyDocGenerator& Docs;
	mutable bool bLoaded = false;
	mutable TArray<FProtoBoyDocNode> Nodes;
	mutable TArray<FProtoBoyDocInfo> Unmapped;
	/** Doc id -> parent id (empty for the root and for docs with no Game doc to hang under). */
	mutable TMap<FString, FString> Parents;
};
