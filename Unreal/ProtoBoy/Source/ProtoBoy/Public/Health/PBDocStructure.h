#pragma once

#include "CoreMinimal.h"

/** Something a structure check found in the docs. */
struct FProtoBoyDocIssue
{
	enum class EKind : uint8
	{
		/** There's no Game doc, so the map has no root. */
		NoGameDoc,
		/** Several Game-layer docs (an old Vision and Overview) that should be combined. */
		SplitGameDocs,
		/** The doc's "protoboy-parent" names a doc that isn't on the map. */
		MissingParent,
		/** [[Wikilinks]] to notes that don't exist. */
		BrokenLinks,
		/** Project paths (Source/..., Content/..., /Game/...) the doc names that don't exist. */
		MissingPaths,
		/** Template sections with nothing written under them. */
		Unwritten,
		/** A Design doc with no Tech or Content doc below it. */
		NoTechOrContent,
		/** Notes in the docs folder that aren't on the map. */
		UnmappedNotes,
		/** Source modules and top-level Content folders no doc mentions. */
		UndocumentedFolders,
	};

	EKind Kind = EKind::Unwritten;
	/** Something is wrong (a broken reference); false for things that are only missing or not filled in yet. */
	bool bWarning = false;
	/** The doc it's about; empty when it's about the docs as a whole. */
	FString DocId;
	FString DocTitle;
	/** Relative to the docs root. */
	FString DocPath;
	FText Message;
	/** What exactly: the links, paths, headings, notes or folders. */
	TArray<FString> Details;
};

/**
 * Structure checks for the docs (Health page > Documentation Structure): what can be told from the docs and the project
 * folder alone, with no agent and no revision control. Check() reads every doc on the map, so call it when the result
 * is needed, not every frame.
 *
 * - Map: no Game doc, split Game docs, a parent that isn't on the map, notes that aren't on the map.
 * - References: wikilinks to notes that don't exist (looked up by note name, in the whole vault when Obsidian is
 *   connected), and project paths that don't exist (Source/, Content/, Config/, Plugins/ and /Game/ paths).
 * - Content: sections with nothing written (hints don't count; Decisions and the Game doc's generated Areas are left
 *   out; the Game doc's example pillar doesn't count as a pillar).
 * - Coverage: Design docs with no Tech or Content doc below them, and source modules or top-level Content folders that
 *   no doc mentions by name.
 *
 * TODO: class and asset names a doc mentions that no longer exist (needs an index of the project's types; engine
 * types that aren't reflected would otherwise all be flagged).
 */
namespace ProtoBoyDocStructure
{
	/** Warnings first, then in map order. Empty when the docs can't be read, or when nothing was found. */
	PROTOBOY_API TArray<FProtoBoyDocIssue> Check();
}
