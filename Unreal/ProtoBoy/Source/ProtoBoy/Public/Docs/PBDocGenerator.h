#pragma once

#include "CoreMinimal.h"
#include "Docs/Obsidian/PBObsidianDocsProvider.h"
#include "Docs/PBDocsProvider.h"

class FProtoBoyObsidianConnector;

struct FProtoBoyDocSearchHit
{
	FString Path;
	FString Title;
	/** A short excerpt around the first match. */
	FString Snippet;
	int32 Score = 0;
};

/** One "### System" entry under "## Systems" in the Architecture doc. */
struct FProtoBoyArchitectureSystem
{
	FString Name;
	/** Everything under the system's heading (owns, code location, dependencies, rules). */
	FString Text;
};

/** The parsed Architecture doc: what agents need to place work correctly. */
struct FProtoBoyArchitecture
{
	bool bExists = false;
	FString EndGoal;
	FString CurrentMilestone;
	FString DependencyRules;
	FString Conventions;
	FString Roadmap;
	TArray<FProtoBoyArchitectureSystem> Systems;
};

/**
 * Pillar 1: living technical & design documentation (Markdown, vault-first).
 *
 * Works through the active IProtoBoyDocsProvider (Obsidian when connected, else the project's DocsDirectory).
 * Docs layout under the docs root:
 *   Architecture doc  systems, dependency rules, conventions, end goal, milestone (humans own it);
 *                     path is UProtoBoySettings::ArchitectureDocPath, default "Architecture.md"
 *   Design/*.md       per-feature design docs
 *   ADR/ADR-NNNN-*.md architecture decision records
 *
 * Docs must survive human edits: writes only append, create new files, or replace ProtoBoy managed blocks.
 */
class PROTOBOY_API FProtoBoyDocGenerator
{
public:
	/** The Architecture doc's path relative to the docs root (UProtoBoySettings::ArchitectureDocPath, normalized). */
	static FString GetArchitectureDocPath();

	/**
	 * Validate, normalize ('/' separators, ".md" added if missing) and save a new Architecture doc path (team-wide).
	 * OutNormalized is the stored value.
	 */
	static bool SetArchitectureDocPath(const FString& Path, FString& OutNormalized, FText& OutError);

	/** A safe docs-relative Markdown path: no drive, no "..", no leading '/'. Adds ".md" if missing. */
	static bool NormalizeDocPath(const FString& Path, FString& OutNormalized, FText& OutError);

	static const TCHAR* DesignFolder;
	static const TCHAR* ADRFolder;

	explicit FProtoBoyDocGenerator(const FProtoBoyObsidianConnector& InObsidian);

	/** The provider docs currently go to. */
	IProtoBoyDocsProvider& GetProvider() const;

	/** Absolute docs root: the Obsidian docs folder when connected, otherwise DocsDirectory from settings. */
	FString GetDocsRoot() const;

	/** Keyword search over titles and text. Empty query returns nothing. */
	TArray<FProtoBoyDocSearchHit> Search(const FString& Query, int32 MaxResults = 10) const;

	/** Create a new doc. Fails if it already exists. */
	bool CreateDoc(const FString& Path, const FString& Text, FText& OutError);

	/** Append to a section (created if missing). Creates the doc, titled after its file name, if it doesn't exist. */
	bool AppendToSection(const FString& Path, const FString& Heading, const FString& Content, FText& OutError);

	/** Replace a ProtoBoy managed block (created inside Heading if missing). Creates the doc if it doesn't exist. */
	bool SetManagedBlock(const FString& Path, const FString& Heading, const FString& BlockId, const FString& Content, FText& OutError);

	bool HasArchitectureDoc() const;
	/** Write the Architecture doc from the template. Fails if it exists. */
	bool CreateArchitectureDoc(FText& OutError);
	FProtoBoyArchitecture ReadArchitecture() const;

	/**
	 * Create ADR/ADR-NNNN-<slug>.md with the next free number. Returns the doc path in OutPath.
	 * ProposedBy names who wrote it (e.g. the agent), shown in the ADR.
	 */
	bool CreateADR(const FString& Title, const FString& Context, const FString& Decision, const FString& Consequences,
		const FString& Status, const FString& ProposedBy, FString& OutPath, FText& OutError);

	/** Create or refresh the TDD entry for an asset. Returns the written file path. */
	FString GenerateTDDStub(const FSoftObjectPath& AssetPath);

private:
	const FProtoBoyObsidianConnector& Obsidian;
	mutable FProtoBoyObsidianDocsProvider ObsidianDocs;
	mutable FProtoBoyLocalDocsProvider LocalDocs;
};
