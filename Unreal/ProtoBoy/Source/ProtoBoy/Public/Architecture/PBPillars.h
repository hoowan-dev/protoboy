#pragma once

#include "CoreMinimal.h"
#include "PBPillars.generated.h"

/**
 * What layer of the game a pillar is. The root is always Game; every other pillar is one of the rest. Types only
 * label pillars to help the team organise the project: any type may sit under any other (only Game is reserved for the
 * root).
 */
UENUM()
enum class EProtoBoyPillarType : uint8
{
	/** Not classified yet. */
	None,
	/** The whole game: only the root. Its doc is the Architecture doc. */
	Game,
	/** Runtime foundations that features are built on; knows nothing about specific features. */
	Core,
	/** Something the player does or experiences, built on Core. */
	Feature,
	/** Authored design data that features run on: what exists in the game and how it's tuned. */
	Content,
	/** How the game communicates with the player: menus, HUD, feedback, onboarding. */
	Interface,
	/** Produced media (models, textures, animation, audio, VFX) and its style, budgets and pipeline. */
	Asset,
	/** Code that helps the team build the game but never ships in it. */
	Tool,
};

/** Display name of a pillar type ("Feature"); empty for None. */
PROTOBOY_API FText GetPillarTypeText(EProtoBoyPillarType Type);
/** One-line description of a pillar type, with examples (for tooltips). */
PROTOBOY_API FText GetPillarTypeDescription(EProtoBoyPillarType Type);
/** The types a pillar below the root can have (everything but Game), in display order. */
PROTOBOY_API TArray<EProtoBoyPillarType> GetPillarTypesForChildren();

/**
 * One architectural pillar: a part of the game the team organises it around. Pillars form a tree (ParentId), and the
 * tree is the dependency graph: a pillar uses its children. The first pillar in the file is the root (the game).
 * Id is stable: it never changes when the pillar is renamed or moved, so docs, tasks and agents can keep referring to it.
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

	/** Parent pillar's Id: the pillar that uses this one. Empty only for the root. */
	UPROPERTY()
	FString ParentId;

	/** What the pillar is. Always Game for the root, never Game for anything else. */
	UPROPERTY()
	EProtoBoyPillarType Type = EProtoBoyPillarType::None;

	/** One-line intent. The long form lives in the pillar's doc. */
	UPROPERTY()
	FString Description;

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

	/**
	 * 2: pillars have an enumerated Type instead of a free-form "kind" string.
	 * 3: the types are Core / Feature / Content / Interface / Asset / Tool (were Domain / Mechanic / Discipline).
	 * 4: a pillar's one-line "vision" is now "description".
	 * 5: no "bRoot" (the first pillar is the root) and no "dependsOn" (parenting is the dependency graph).
	 * Older files are migrated on load and saved in the current format on the next edit.
	 */
	static constexpr int32 CurrentVersion = 5;

	UPROPERTY()
	int32 Version = CurrentVersion;

	UPROPERTY()
	TArray<FProtoBoyPillar> Pillars;

	UPROPERTY()
	TArray<FProtoBoyArchivedPillar> Archived;
};

/**
 * Owns the pillar tree: loads it from and saves it to the pillar file, and keeps it valid (unique ids, no cycles).
 * Every edit saves immediately and fires OnChanged, and can be undone and redone (per editor session; reloading the
 * file clears the history). Doc bookkeeping (doc paths, archived pillars) isn't part of the history: undo keeps docs
 * that were generated, and archives the docs of pillars it removes.
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
	/**
	 * The root pillar: the game, named after the project, always the first pillar. Every other pillar sits below it.
	 * It can't be removed, moved or renamed, and has no pillar doc of its own (the Architecture doc stands for it).
	 */
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
	 * Apply edits to name, type and description. Id, parent, order and doc path are changed elsewhere. The root keeps
	 * the project's name and type; other pillars can't become Game.
	 */
	bool UpdatePillar(const FProtoBoyPillar& Edited, FText& OutError);
	/** Move under a new parent (empty = the root). Refuses moving the root and moves into its own subtree. */
	bool SetParent(const FString& Id, const FString& NewParentId, FText& OutError);
	/**
	 * Move under NewParentId (empty = the root) at Index among its new siblings, counted without the moved pillar
	 * (INDEX_NONE = last). Refuses moving the root and moves into its own subtree. Used by drag and drop.
	 */
	bool MovePillar(const FString& Id, const FString& NewParentId, int32 Index, FText& OutError);
	/** Move up (-1) or down (+1) among siblings. */
	void MoveAmongSiblings(const FString& Id, int32 Delta);
	/** Remove a pillar and everything below it (never the root). Their docs are archived on the next generation. */
	void RemovePillar(const FString& Id);
	/** Remove every pillar below the root, leaving just the game. Their docs are archived on the next generation. */
	void ClearPillars();
	/**
	 * Replace every pillar below the root with Pillars (a template's tree: its first pillar, the root, is skipped, and
	 * its top-level pillars go under this tree's root). Ids that clash with archived pillars get a suffix; parents
	 * follow. The removed pillars' docs are archived on the next generation. Saves once.
	 */
	void ReplacePillars(const TArray<FProtoBoyPillar>& Pillars);

	/** Record a pillar's generated doc. */
	void SetDocPath(const FString& Id, const FString& DocPath);
	/** Forget an archived pillar once its doc has been marked. */
	void ClearArchived(const FString& Id);

	// Undo / redo
	bool CanUndo() const { return !UndoStack.IsEmpty(); }
	bool CanRedo() const { return !RedoStack.IsEmpty(); }
	/** What the next undo / redo reverts or reapplies, e.g. "Add Pillar"; empty when there's nothing to do. */
	FText GetUndoDescription() const { return CanUndo() ? UndoStack.Last().Description : FText::GetEmpty(); }
	FText GetRedoDescription() const { return CanRedo() ? RedoStack.Last().Description : FText::GetEmpty(); }
	void Undo();
	void Redo();

	/** Why the last load or save failed; empty when fine. */
	const FText& GetLastError() const { return LastError; }

	FSimpleMulticastDelegate OnChanged;

private:
	FProtoBoyPillar* FindMutable(const FString& Id);
	/**
	 * Make the first pillar the root (type Game, named after the project), with every other pillar below it. Run after
	 * every load: files from before the root existed get one, and their top-level pillars (or pillars whose parent is
	 * missing) move under it. Doesn't save; the next edit does.
	 */
	void EnsureRoot();
	/** Remove these pillars (no save), clean up dependencies on them, and archive the ones that have docs. */
	void RemoveAndArchive(const TArray<FString>& Removed);
	/** Empty -> the root; otherwise the Id as given. */
	FString ResolveParent(const FString& ParentId) const;
	FString MakeUniqueId(const FString& Name) const;
	void Renumber(const FString& ParentId);
	/** Record an edit for undo (if the pillars changed since the last save), then save and notify. */
	void CommitEdit(const FText& Description);
	/** Save and notify without touching the history: doc bookkeeping, undo and redo. */
	void SaveAndNotify();

	/** One step of history: the pillars before (undo) or after (redo) an edit. */
	struct FHistoryEntry
	{
		TArray<FProtoBoyPillar> Pillars;
		FText Description;
	};
	static constexpr int32 MaxHistory = 100;
	/** Swap the tree's pillars for Pillars, carrying doc bookkeeping over (see the class comment). */
	void RestorePillars(const TArray<FProtoBoyPillar>& Pillars);

	FProtoBoyPillarTree Tree;
	/** The pillars as of the last load or save: the "before" state of the next edit. */
	TArray<FProtoBoyPillar> CommittedPillars;
	TArray<FHistoryEntry> UndoStack;
	TArray<FHistoryEntry> RedoStack;
	FText LastError;
};
