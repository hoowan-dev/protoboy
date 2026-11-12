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

	/** Parent pillar's Id; empty for a top-level pillar. */
	UPROPERTY()
	FString ParentId;

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

	const TArray<FProtoBoyPillar>& GetPillars() const { return Tree.Pillars; }
	const TArray<FProtoBoyArchivedPillar>& GetArchived() const { return Tree.Archived; }
	const FProtoBoyPillar* Find(const FString& Id) const;

	/** Children of ParentId (empty = top level), in order. */
	TArray<const FProtoBoyPillar*> GetChildren(const FString& ParentId) const;
	/** Ancestors of Id, top-level first (not including Id). */
	TArray<const FProtoBoyPillar*> GetAncestors(const FString& Id) const;
	/** True if Id is Ancestor or sits anywhere below it. */
	bool IsInSubtree(const FString& Id, const FString& Ancestor) const;
	/** "Gameplay / Combat" */
	FString GetDisplayPath(const FString& Id) const;

	/** Add a pillar at the end of ParentId's children. Returns its new Id. */
	FString AddPillar(const FString& Name, const FString& ParentId);
	/** Add a starter set of top-level pillars (Gameplay, Animation, Art, Audio, Content, UI). Skips names that exist. */
	void AddStarterPillars();

	/** Apply edits to name, kind, vision and dependencies. Id, parent, order and doc path are changed elsewhere. */
	bool UpdatePillar(const FProtoBoyPillar& Edited, FText& OutError);
	/** Move under a new parent (empty = top level). Refuses moves into its own subtree. */
	bool SetParent(const FString& Id, const FString& NewParentId, FText& OutError);
	/**
	 * Move under NewParentId (empty = top level) at Index among its new siblings, counted without the moved pillar
	 * (INDEX_NONE = last). Refuses moves into its own subtree. Used by drag and drop.
	 */
	bool MovePillar(const FString& Id, const FString& NewParentId, int32 Index, FText& OutError);
	/** Move up (-1) or down (+1) among siblings. */
	void MoveAmongSiblings(const FString& Id, int32 Delta);
	/** Remove a pillar and everything below it. Their docs are archived on the next generation. */
	void RemovePillar(const FString& Id);

	/** Record a pillar's generated doc. */
	void SetDocPath(const FString& Id, const FString& DocPath);
	/** Forget an archived pillar once its doc has been marked. */
	void ClearArchived(const FString& Id);

	/** Why the last load or save failed; empty when fine. */
	const FText& GetLastError() const { return LastError; }

	FSimpleMulticastDelegate OnChanged;

private:
	FProtoBoyPillar* FindMutable(const FString& Id);
	FString MakeUniqueId(const FString& Name) const;
	void Renumber(const FString& ParentId);
	void SaveAndNotify();

	FProtoBoyPillarTree Tree;
	FText LastError;
};
