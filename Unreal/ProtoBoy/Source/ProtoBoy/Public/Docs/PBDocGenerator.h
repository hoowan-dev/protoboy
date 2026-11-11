#pragma once

#include "CoreMinimal.h"

/** Pillar 1: living technical & design documentation (Markdown, vault-first). */
class PROTOBOY_API FProtoBoyDocGenerator
{
public:
	/** Absolute path of the docs root folder from settings. */
	FString GetDocsRoot() const;

	/** Create or refresh the TDD entry for an asset. Returns the written file path. */
	FString GenerateTDDStub(const FSoftObjectPath& AssetPath);

	/** Create a new Architectural Decision Record. Returns the written file path. */
	FString CreateADR(const FString& Title, const FString& Why);
};
