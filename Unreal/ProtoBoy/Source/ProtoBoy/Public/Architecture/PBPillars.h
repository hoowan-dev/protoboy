#pragma once

#include "CoreMinimal.h"
#include "PBPillars.generated.h"

/**
 * One architectural pillar: a mechanic, domain, discipline or anything else the team organises the game around.
 * Pillars form a tree (ParentId). Id is stable: it never changes when the pillar is renamed or moved, so docs, tasks
 * and agents can keep referring to it.
 */
USTRUCT()
struct PROTOBOY_API FProtoBoyPillar
{
	GENERATED_BODY()

	/** Stable identifier, e.g. "combat". Generated from the first name; never changes. */
	UPROPERTY()
	FString Id;

	UPROPERTY()
	FString Name;

	/** Parent pillar's Id; empty only for the root. */
	UPROPERTY()
	FString ParentId;

	/**
	 * The game itself: the single root of the tree, named after the project. Every other pillar sits below it. It
	 * can't be removed, moved or renamed, and has no pillar doc of its own (the Architecture doc stands for it).
	 */
	UPROPERTY()
	bool bRoot = false;

	/** Free-form category chosen by the team, e.g. "Domain", "Mechanic", "Discipline". */
	UPROPERTY()
	FString Kind;

	/** One-line intent. The long form lives in the pillar's doc. */
	UPROPERTY()
	FString Vision;

	/** Ids of pillars this one may depend on. */
	UPROPERTY()
	TArray<FString> DependsOn;

	/** Position among its siblings. */
	UPROPERTY()
	int32 Order = 0;

	/** The pillar's doc, relative to the docs folder. Set when the doc is first generated; kept across renames. */
	UPROPERTY()
	FString DocPath;
};

/** A removed pillar whose doc still has to be marked as archived on the next doc generation. */
USTRUCT()
struct PROTOBOY_API FProtoBoyArchivedPillar
{
	GENERATED_BODY()

	UPROPERTY()
	FString Id;

	UPROPERTY()
	FString Name;

	UPROPERTY()
	FString DocPath;

	/** UTC, ISO 8601. */
	UPROPERTY()
	FString RemovedUtc;
};

/** The pillar tree file (UProtoBoySettings::PillarsFilePath, committed and shared by the team). */
USTRUCT()
struct PROTOBOY_API FProtoBoyPillarTree
{
	GENERATED_BODY()

	static constexpr int32 CurrentVersion = 1;

	UPROPERTY()
	int32 Version = CurrentVersion;

	UPROPERTY()
	TArray<FProtoBoyPillar> Pillars;

	UPROPERTY()
	TArray<FProtoBoyArchivedPillar> Archived;
};

/**
 * Owns the pillar tree: loads it from and saves it to the pillar file, and keeps it valid (unique ids, no cycles).
 * Every edit saves immediately and fires OnChanged.
 */
class PROTOBOY_API FProtoBoyPillarService
{
public:
	FProtoBoyPillarService();

	/** Absolute path of the pillar file. */
	static FString GetFilePath();

	/** Re-read the file (e.g. after pulling changes). An empty tree if there's no file yet. */
	bool Reload(FText& OutError);

	/** Every pillar, the root included. */
	const TArray<FProtoBoyPillar>& GetPillars() const { return Tree.Pillars; }
	/** True when the team has added pillars below the root. */
	bool HasPillars() const { return Tree.Pillars.Num() > 1; }
	/** The root pillar (the game). Always exists. */
	const FProtoBoyPillar& GetRoot() const;
	const FString& GetRootId() const { return GetRoot().Id; }
	bool IsRoot(const FString& Id) const { return !Id.IsEmpty() && Id == GetRootId(); }
	const TArray<FProtoBoyArchivedPillar>& GetArchived() const { return Tree.Archived; }
	const FProtoBoyPillar* Find(const FString& Id) const;

	/** Children of ParentId, in order. GetChildren("") is just the root. */
	TArray<const FProtoBoyPillar*> GetChildren(const FString& ParentId) const;
	/** Ancestors of Id, the root first (not including Id). */
	TArray<const FProtoBoyPillar*> GetAncestors(const FString& Id) const;
	/** True if Id is Ancestor or sits anywhere below it. */
	bool IsInSubtree(const FString& Id, const FString& Ancestor) const;
	/** "Gameplay / Combat" (the root is left out, except for the root itself). */
	FString GetDisplayPath(const FString& Id) const;

	/** Add a pillar at the end of ParentId's children (empty or unknown = under the root). Returns its new Id. */
	FString AddPillar(const FString& Name, const FString& ParentId);

	/**
	 * Apply edits to name, kind, vision and dependencies. Id, parent, order and doc path are changed elsewhere.
	 * The root keeps the project's name.
	 */
	bool UpdatePillar(const FProtoBoyPillar& Edited, FText& OutError);
	/** Move under a new parent (empty = the root). Refuses moving the root, and moves into its own subtree. */
	bool SetParent(const FString& Id, const FString& NewParentId, FText& OutError);
	/**
	 * Move under NewParentId (empty = the root) at Index among its new siblings, counted without the moved pillar
	 * (INDEX_NONE = last). Refuses moving the root, and moves into its own subtree. Used by drag and drop.
	 */
	bool MovePillar(const FString& Id, const FString& NewParentId, int32 Index, FText& OutError);
	/** Move up (-1) or down (+1) among siblings. */
	void MoveAmongSiblings(const FString& Id, int32 Delta);
	/** Remove a pillar and everything below it (never the root). Their docs are archived on the next generation. */
	void RemovePillar(const FString& Id);
	/** Remove every pillar below the root, leaving just the game. Their docs are archived on the next generation. */
	void ClearPillars();
	/**
	 * Replace every pillar below the root with Pillars (a template's tree: its root is skipped, and its top-level
	 * pillars go under this tree's root). Ids that clash with archived pillars get a suffix; parents and dependencies
	 * follow. The removed pillars' docs are archived on the next generation. Saves once.
	 */
	void ReplacePillars(const TArray<FProtoBoyPillar>& Pillars);

	/** Record a pillar's generated doc. */
	void SetDocPath(const FString& Id, const FString& DocPath);
	/** Forget an archived pillar once its doc has been marked. */
	void ClearArchived(const FString& Id);

	/** Why the last load or save failed; empty when fine. */
	const FText& GetLastError() const { return LastError; }

	FSimpleMulticastDelegate OnChanged;

private:
	FProtoBoyPillar* FindMutable(const FString& Id);
	/**
	 * Make the tree have exactly one root, named after the project, with every other pillar below it. Run after every
	 * load: files from before the root existed get one, and their top-level pillars (or pillars whose parent is
	 * missing) move under it. Doesn't save; the next edit does.
	 */
	void EnsureRoot();
	/** Remove these pillars (no save), clean up dependencies on them, and archive the ones that have docs. */
	void RemoveAndArchive(const TArray<FString>& Removed);
	/** Empty -> the root; otherwise the Id as given. */
	FString ResolveParent(const FString& ParentId) const;
	FString MakeUniqueId(const FString& Name) const;
	void Renumber(const FString& ParentId);
	void SaveAndNotify();

	FProtoBoyPillarTree Tree;
	FText LastError;
};
