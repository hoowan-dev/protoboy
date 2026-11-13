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

/**
 * Pillar 1: living technical & design documentation (Markdown, vault-first).
 *
 * Works through the active IProtoBoyDocsProvider (Obsidian when connected, else the project's DocsDirectory).
 * The docs are organised in layers (Game, Design, Tech, Content; see PBDocLayers.h), mapped by FProtoBoyDocMap.
 * This class is the plain read/write/search layer under it.
 *
 * Docs must survive human edits: writes only append, create new files, or replace ProtoBoy managed blocks.
 */
class PROTOBOY_API FProtoBoyDocGenerator
{
public:
	/** A safe docs-relative Markdown path: no drive, no "..", no leading '/'. Adds ".md" if missing. */
	static bool NormalizeDocPath(const FString& Path, FString& OutNormalized, FText& OutError);


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

	/** Create or refresh the TDD entry for an asset. Returns the written file path. */
	FString GenerateTDDStub(const FSoftObjectPath& AssetPath);

private:
	const FProtoBoyObsidianConnector& Obsidian;
	mutable FProtoBoyObsidianDocsProvider ObsidianDocs;
	mutable FProtoBoyLocalDocsProvider LocalDocs;
};
