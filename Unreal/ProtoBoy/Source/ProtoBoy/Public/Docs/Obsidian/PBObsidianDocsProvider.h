#pragma once

#include "Docs/PBDocsProvider.h"

class FProtoBoyObsidianConnector;

/** Docs in the connected Obsidian vault's docs folder. Plain files, so Obsidian picks up changes live. */
class PROTOBOY_API FProtoBoyObsidianDocsProvider : public FProtoBoyFolderDocsProvider
{
public:
	explicit FProtoBoyObsidianDocsProvider(const FProtoBoyObsidianConnector& InConnector);

	virtual FText GetDisplayName() const override;
	virtual FString GetDocsRoot() const override;
	virtual bool IsReady(FText& OutWhyNot) const override;
	/** The vault's .trash folder, which Obsidian treats as its own trash. */
	virtual FString GetTrashDir() const override;
	virtual FText GetDeletedDocsLocation() const override;

private:
	const FProtoBoyObsidianConnector& Connector;
};
