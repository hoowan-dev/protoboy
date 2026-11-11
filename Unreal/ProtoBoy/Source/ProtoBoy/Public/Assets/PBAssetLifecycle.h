#pragma once

#include "CoreMinimal.h"

enum class EProtoBoyAssetLifecycle : uint8
{
	Untracked,
	Prototype,
	Production,
};

/** Pillar 3: asset provenance and Prototype/Temp vs Production Ready tagging. */
class PROTOBOY_API FProtoBoyAssetLifecycle
{
public:
	EProtoBoyAssetLifecycle GetLifecycle(const FSoftObjectPath& AssetPath) const;
	void SetLifecycle(const FSoftObjectPath& AssetPath, EProtoBoyAssetLifecycle Lifecycle);

	/** Write the "Greybox Replacement Matrix" of all Prototype assets. Returns the file path. */
	FString WriteReplacementMatrix() const;

private:
	// TODO: Persist to a version-controlled manifest (e.g. ProtoBoy/AssetLifecycle.json).
	TMap<FSoftObjectPath, EProtoBoyAssetLifecycle> Entries;
};
