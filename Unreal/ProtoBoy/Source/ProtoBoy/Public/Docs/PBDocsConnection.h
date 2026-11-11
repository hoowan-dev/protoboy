#pragma once

#include "CoreMinimal.h"
#include "PBDocsConnection.generated.h"

/** Documentation platforms ProtoBoy can connect to. */
UENUM()
enum class EProtoBoyDocPlatform : uint8
{
	None = 0,
	Obsidian = 1,
};

/**
 * This user's documentation connection, serialized as JSON to
 * <Project>/Saved/ProtoBoy/DocsConnection.json (per user, never committed).
 * Add fields freely: unknown/missing fields are ignored on load. Bump Version for breaking changes.
 */
USTRUCT()
struct PROTOBOY_API FProtoBoyDocsConnection
{
	GENERATED_BODY()

	static constexpr int32 CurrentVersion = 1;

	UPROPERTY()
	int32 Version = CurrentVersion;

	UPROPERTY()
	EProtoBoyDocPlatform Platform = EProtoBoyDocPlatform::None;

	/** Obsidian: absolute vault folder ('/' separators). */
	UPROPERTY()
	FString VaultPath;

	/** Obsidian: docs folder relative to the vault root. */
	UPROPERTY()
	FString DocsFolder;

	/** UTC, ISO 8601. */
	UPROPERTY()
	FString ConnectedAtUtc;

	bool IsSet() const { return Platform != EProtoBoyDocPlatform::None; }
};

/** Loads/saves FProtoBoyDocsConnection. */
class PROTOBOY_API FProtoBoyDocsConnectionStore
{
public:
	static FString GetFilePath();

	/** False (and Out reset) if there is no file or it can't be read. */
	static bool Load(FProtoBoyDocsConnection& Out);

	static bool Save(const FProtoBoyDocsConnection& Connection);

	/** Delete the file. */
	static void Clear();
};
