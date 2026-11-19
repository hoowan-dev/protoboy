#pragma once

#include "CoreMinimal.h"
#include "Docs/PBDocLayers.h"

/** A project file that changed in revision control (or on disk, uncommitted) since the oldest doc was last edited. */
struct FProtoBoyChangedFile
{
	/** Relative to the project folder, '/' separators, e.g. "Source/Game/Hero.cpp". */
	FString Path;
	/** Its latest change: the newest commit's date, or the file's own date when it has uncommitted changes. */
	FDateTime ChangedUtc;
	/** Commits that touched it since the oldest doc was edited (0: only uncommitted changes). */
	int32 NumCommits = 0;
	/** Modified, added or deleted locally and not committed yet. */
	bool bUncommitted = false;
	/** The newest commit's first line; empty when there's none. */
	FString Subject;
};

/** A doc on the map with project files that changed after it was last edited. */
struct FProtoBoyDocChanges
{
	FString DocId;
	/** Relative to the docs root. */
	FString Path;
	FString Title;
	EProtoBoyDocLayer Layer = EProtoBoyDocLayer::Design;
	/** When the doc's file was last written, or when it was last marked as up to date (FProtoBoyDocNode::ReviewedUtc) if later. */
	FDateTime ModifiedUtc;
	/** How many of FProtoBoyDocHealthChanges::Files changed after ModifiedUtc: the first NumChanged, as they're newest first. */
	int32 NumChanged = 0;
	/** Indices into Files of the changed files the doc's text names (a file or class name, or a folder path), newest first. */
	TArray<int32> Named;
};

/** What changed in the project since the docs were last edited: the input to the health check's agent (FProtoBoyDocHealthRun). */
struct FProtoBoyDocHealthChanges
{
	/** Newest first. */
	TArray<FProtoBoyChangedFile> Files;
	/** Only the docs with at least one file changed after them, in map order. */
	TArray<FProtoBoyDocChanges> Docs;
	/** Every doc on the map that was looked at. */
	int32 NumDocsChecked = 0;
};

/**
 * The deterministic half of the documentation health checks: which project files changed since each doc on the map was
 * last edited, and which of them each doc names.
 *
 * A doc's "last edited" is its file's modified date, not its revision control history: docs often live outside the
 * repository (an Obsidian vault). A doc marked as up to date (FProtoBoyDocMap::MarkReviewed) counts from that date
 * when it's later. Changes come from Git only for now (`git log` since the oldest doc, plus uncommitted
 * and untracked files), limited to the project's Source, Content, Plugins and Config folders. Git is run from the PATH
 * in the project folder and blocks while it runs.
 *
 * Known gaps: a commit made before a doc was edited but pulled after it isn't counted (commits are dated by when they
 * were made), and a doc whose frontmatter ProtoBoy rewrote (moved on the map) looks freshly edited.
 *
 * TODO: other providers (Perforce, Unity Version Control, Subversion) through ISourceControlProvider.
 */
namespace ProtoBoyDocHealthChanges
{
	/** False with a reason unless the editor's revision control provider is one Collect can read history from (Git). */
	PROTOBOY_API bool IsProviderSupported(FText& OutWhyNot);

	/** False with a reason if the docs or the history can't be read. An empty Out.Docs means nothing changed since the docs were edited. */
	PROTOBOY_API bool Collect(FProtoBoyDocHealthChanges& Out, FText& OutError);
}
