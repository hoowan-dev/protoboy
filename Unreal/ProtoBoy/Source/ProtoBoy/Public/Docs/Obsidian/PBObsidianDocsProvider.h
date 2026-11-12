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

private:
	const FProtoBoyObsidianConnector& Connector;
};
