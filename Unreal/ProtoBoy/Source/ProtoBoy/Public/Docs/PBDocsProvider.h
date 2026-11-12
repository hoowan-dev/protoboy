#pragma once

#include "CoreMinimal.h"

/** A Markdown document in the docs root. */
struct FProtoBoyDocInfo
{
	/** Path relative to the docs root, '/' separators, e.g. "Design/Combat.md". */
	FString Path;
	/** First "# " heading, or the file name. */
	FString Title;
	FDateTime ModifiedUtc;
	int64 SizeBytes = 0;
};

/**
 * Where living docs are stored (Obsidian vault, project folder; later Notion, Confluence, ...).
 * Paths are always relative to the docs root and must be .md files inside it.
 * Higher-level rules (sections, managed blocks, templates) live in FProtoBoyDocGenerator, not here.
 */
class PROTOBOY_API IProtoBoyDocsProvider
{
public:
	virtual ~IProtoBoyDocsProvider() = default;

	/** Shown to users and agents, e.g. "Obsidian (vault \"Hoowan\")". */
	virtual FText GetDisplayName() const = 0;

	/** False with a reason if docs can't be used right now. */
	virtual bool IsReady(FText& OutWhyNot) const = 0;

	virtual TArray<FProtoBoyDocInfo> ListDocs() const = 0;
	virtual bool DocExists(const FString& RelativePath) const = 0;
	virtual bool ReadDoc(const FString& RelativePath, FString& OutText, FText& OutError) const = 0;

	/** Write a doc, creating folders as needed. bMustNotExist fails if the file already exists. */
	virtual bool WriteDoc(const FString& RelativePath, const FString& Text, bool bMustNotExist, FText& OutError) = 0;
};

/** Docs stored as Markdown files in a folder on disk. GetDocsRoot decides which folder. */
class PROTOBOY_API FProtoBoyFolderDocsProvider : public IProtoBoyDocsProvider
{
public:
	/** Absolute docs root, '/' separators. */
	virtual FString GetDocsRoot() const = 0;

	virtual bool IsReady(FText& OutWhyNot) const override;
	virtual TArray<FProtoBoyDocInfo> ListDocs() const override;
	virtual bool DocExists(const FString& RelativePath) const override;
	virtual bool ReadDoc(const FString& RelativePath, FString& OutText, FText& OutError) const override;
	virtual bool WriteDoc(const FString& RelativePath, const FString& Text, bool bMustNotExist, FText& OutError) override;

	/** Validate a relative .md path and turn it into an absolute one inside the root. */
	bool ResolvePath(const FString& RelativePath, FString& OutAbsolutePath, FText& OutError) const;

	/** Largest doc read or written (keeps agent payloads sane). */
	static constexpr int64 MaxDocBytes = 1024 * 1024;
};

/** Docs in the project folder (UProtoBoySettings::DocsDirectory), used when no docs platform is connected. */
class PROTOBOY_API FProtoBoyLocalDocsProvider : public FProtoBoyFolderDocsProvider
{
public:
	virtual FText GetDisplayName() const override;
	virtual FString GetDocsRoot() const override;
};
