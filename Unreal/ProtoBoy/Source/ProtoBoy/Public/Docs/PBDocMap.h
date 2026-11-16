#pragma once

#include "CoreMinimal.h"
#include "Docs/PBDocLayers.h"
#include "Docs/PBDocsProvider.h"

class FProtoBoyDocsService;
struct FProtoBoyDocTemplate;
struct FProtoBoyDocTemplatePillar;
struct FProtoBoyDocTemplateSection;

/** One doc on the map, read from its frontmatter (see ProtoBoyDocLayers for the keys). */
struct FProtoBoyDocNode
{
	/** Stable id from the frontmatter; derived from the path when the doc has a layer but no id. */
	FString Id;
	/** Relative to the docs root, '/' separators. */
	FString Path;
	FString Title;
	EProtoBoyDocLayer Layer = EProtoBoyDocLayer::Design;
	/** The layer as written ("vision", "systems", ... from older versions map onto today's layers). */
	FString LayerKey;
	/**
	 * The parent's id as written in the frontmatter (or an old "protoboy-implements" when there's no parent key). May be
	 * empty or stale: use FProtoBoyDocMap::GetParent.
	 */
	FString ParentId;
	/** The first line someone actually wrote below the title (hints and quotes skipped), for previews. */
	FString Summary;
	/** Marked as a task category ("protoboy-task-category: true"), so tasks can be grouped under it. */
	bool bTaskCategory = false;
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
	};

	/** The template's docs below the Game doc, in order. */
	TArray<FEntry> Docs;
	/** Docs that would be written: the Game doc plus Docs. */
	int32 NumToCreate = 0;
	/**
	 * Paths of the docs moved to the trash first: every doc on the map, plus any other file at a path the template
	 * writes. Other notes in the docs folder are left alone.
	 */
	TArray<FString> Removed;
};

/**
 * The documentation map: every doc with a layer in its frontmatter, as a tree. The docs are the source of truth:
 * the map is rebuilt by reading them (Refresh), so edits made in Obsidian show up after a refresh, and everything
 * ProtoBoy changes is written into the docs themselves (frontmatter, new files, the Game doc's managed Areas block).
 *
 * Tree: the Game doc is the root and never goes under anything. Every other doc's parent is its "protoboy-parent" when
 * that names another doc (and wouldn't make a loop), otherwise the Game doc. Nothing restricts which layer goes under
 * which: the team puts each doc wherever makes sense to them.
 *
 * Docs from older versions keep working: a Vision and an Overview are both Game docs (the Vision is the root, the
 * Overview hangs under it), Systems docs are Tech, Direction docs are Content, and a feature Tech doc's
 * "protoboy-implements" counts as its parent. ADRs ("decision", or in the ADR folder) stay off the map: decisions now
 * live in each doc's Decisions section (RecordDecision).
 *
 * Implemented in PBDocMap.cpp (loading, the tree, editing), PBDocMapTemplates.cpp (ClearDocs, PlanTemplate,
 * ApplyTemplate) and PBDocMapMigration.cpp (CombineGameDocs); shared helpers are in Private/Docs/PBDocMapUtils.h.
 */
class PROTOBOY_API FProtoBoyDocMap
{
public:
	explicit FProtoBoyDocMap(FProtoBoyDocsService& InDocs);

	/** Re-read every doc. Fires OnChanged. */
	void Refresh();

	const TArray<FProtoBoyDocNode>& GetNodes() const;
	/** Docs in the docs folder without a layer (not on the map), e.g. notes written before ProtoBoy. ADRs aren't listed. */
	const TArray<FProtoBoyDocInfo>& GetUnmappedDocs() const;

	const FProtoBoyDocNode* Find(const FString& Id) const;
	const FProtoBoyDocNode* FindByPath(const FString& Path) const;
	/** The Game doc: the map's root. With several, a "game" one first, then an old Vision, then an old Overview. */
	const FProtoBoyDocNode* GetGame() const;
	bool HasGameDoc() const { return GetGame() != nullptr; }
	/** Every Game-layer doc, the root first. More than one means an old Vision and Overview that CombineGameDocs can merge. */
	TArray<const FProtoBoyDocNode*> GetGameDocs() const;
	bool HasSplitGameDocs() const { return GetGameDocs().Num() > 1; }
	/** The project's name: the Game doc's title, and the root node's. */
	static FString GetProjectTitle();
	/** Where a new Game doc goes: "<Project>.md" at the docs root. An existing Game doc keeps whatever file it's in. */
	static FString GetGameDocPath();

	/** The doc this one sits under (see the class comment). Null for the Game doc, and when there's no Game doc. */
	const FProtoBoyDocNode* GetParent(const FString& Id) const;
	/** The parent a doc gets when its frontmatter names none: the Game doc (null for the Game doc itself). */
	const FProtoBoyDocNode* GetDefaultParent(const FProtoBoyDocNode& Node) const;
	/** Docs directly under Id, in layer order, then by title. */
	TArray<const FProtoBoyDocNode*> GetChildren(const FString& Id) const;
	/** True if Id is Ancestor or sits anywhere below it. */
	bool IsInSubtree(const FString& Id, const FString& Ancestor) const;
	/** Create the Game doc (<Project>.md, titled with the project's name) from its template, if there isn't one. */
	bool CreateGameDoc(FText& OutError);

	/**
	 * Merge every Game-layer doc (an old Vision and Overview) into one <Project>.md titled with the project's name: each Game
	 * section takes the first written version (the root's first), sections the template doesn't have are kept after
	 * them, Decisions are concatenated. The other docs go to the trash (IProtoBoyDocsProvider::DeleteDoc), and docs that
	 * named them as their parent go under the new Game doc.
	 */
	bool CombineGameDocs(FText& OutError);

	/**
	 * Delete a doc (the provider moves it to its trash). Docs whose frontmatter put them under it go back to their
	 * default place. The Game doc can't be deleted.
	 */
	bool DeleteDoc(const FString& Id, FText& OutError);

	/**
	 * Create a doc of Kind (one of ProtoBoyDocLayers::GetAddableKinds) titled Title under ParentId (empty = the Game
	 * doc), from its template. The file follows the parent: under the Game doc it goes in its kind's folder (Design/,
	 * Tech/, Content/), under any other doc in that doc's folder (Content/Ammo.md's children in Content/Ammo/). Design
	 * docs get a folder of their own (Design/Combat/Weapons/Weapons.md).
	 */
	bool CreateDoc(EProtoBoyDocKind Kind, const FString& Title, const FString& ParentId, FString& OutId, FText& OutError);

	/** Move a doc under another (its frontmatter changes; the file stays where it is). Refuses loops and moving the Game doc. */
	bool SetParent(const FString& Id, const FString& NewParentId, FText& OutError);

	/** Mark or unmark a doc as a task category, in its frontmatter ("protoboy-task-category: true", removed when false). */
	bool SetTaskCategory(const FString& Id, bool bTaskCategory, FText& OutError);
	/** Docs marked as task categories, in map order (depth first from the Game doc). */
	TArray<const FProtoBoyDocNode*> GetTaskCategories() const;

	/**
	 * False with a reason for titles no doc may have: "Uncategorized" is the task board's catch-all category
	 * (ProtoBoyTasks::UncategorizedName). Checked by CreateDoc, AddToMap and ApplyTemplate.
	 */
	static bool IsAllowedTitle(const FString& Title, FText& OutWhyNot);

	/**
	 * Put an existing doc on the map under ParentId (empty = the Game doc) by adding ProtoBoy's frontmatter; its text is
	 * left alone. Refused for a second Game doc.
	 */
	bool AddToMap(const FString& Path, EProtoBoyDocLayer Layer, const FString& ParentId, FText& OutError);

	/**
	 * Append a dated entry to a doc's Decisions section (created at the end if missing): what was decided, why, and what
	 * it rules out. By names who recorded it (e.g. "Claude Code via ProtoBoy").
	 */
	bool RecordDecision(const FString& Id, const FString& Decision, const FString& Why, const FString& RulesOut, const FString& By, FText& OutError);

	/**
	 * Move every doc on the map (the Game doc included) to the provider's trash and remove the folders that leaves empty.
	 * Notes that aren't on the map are kept. For replacing the docs with generated ones (FProtoBoyDocGenerationRun).
	 */
	bool ClearDocs(FText& OutError);

	/** What ApplyTemplate would remove and create. */
	FProtoBoyDocTemplatePlan PlanTemplate(const FProtoBoyDocTemplate& Template) const;

	/**
	 * Replace the documentation with a template's: every doc on the map (the Game doc included) goes to the provider's
	 * trash, then a new Game doc (with the template's design pillars) and the template's docs are written, in the same places CreateDoc would put them. Notes that aren't on the map are kept, unless one sits where a template doc goes.
	 * With bExamples, each doc's sections also get the template's example content (otherwise just the headings and hints).
	 * OutCreated counts the docs written.
	 */
	bool ApplyTemplate(const FProtoBoyDocTemplate& Template, bool bExamples, int32& OutCreated, FText& OutError);

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
	/**
	 * Write <Project>.md if there's no Game doc, without refreshing. Pillars (from a template) replace the example pillar;
	 * Examples (from a template, optional) fill its sections.
	 */
	bool WriteMissingGameDoc(const TArray<FProtoBoyDocTemplatePillar>* Pillars, const FString& TemplateName, bool& bOutWrote, FText& OutError,
		const TArray<FProtoBoyDocTemplateSection>* Examples = nullptr, bool bGenerated = false);

	FProtoBoyDocsService& Docs;
	mutable bool bLoaded = false;
	mutable TArray<FProtoBoyDocNode> Nodes;
	mutable TArray<FProtoBoyDocInfo> Unmapped;
	/** Doc id -> parent id (empty for the root and for docs with no Game doc to hang under). */
	mutable TMap<FString, FString> Parents;
};
