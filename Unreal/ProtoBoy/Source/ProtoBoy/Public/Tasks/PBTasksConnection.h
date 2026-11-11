#pragma once

#include "CoreMinimal.h"
#include "PBTasksConnection.generated.h"

/** Kanban-style task board providers ProtoBoy can connect to. Values double as the Connect Task Board window's page indices. */
UENUM()
enum class EProtoBoyTaskProvider : uint8
{
	None = 0,
	Trello = 1,
};

/**
 * This user's task board connection, serialized as JSON to
 * <Project>/Saved/ProtoBoy/TasksConnection.json (per user, never committed: it holds the access token).
 * Add fields freely: unknown/missing fields are ignored on load. Bump Version for breaking changes.
 */
USTRUCT()
struct PROTOBOY_API FProtoBoyTasksConnection
{
	GENERATED_BODY()

	static constexpr int32 CurrentVersion = 1;

	UPROPERTY()
	int32 Version = CurrentVersion;

	UPROPERTY()
	EProtoBoyTaskProvider Provider = EProtoBoyTaskProvider::None;

	/** Trello: user token granted on trello.com/1/authorize (never expires; revoked on Disconnect). */
	UPROPERTY()
	FString TrelloToken;

	/** Trello: the API key the token was granted to. A token only works with that key. */
	UPROPERTY()
	FString TrelloApiKey;

	UPROPERTY()
	FString TrelloMemberId;

	UPROPERTY()
	FString TrelloUsername;

	UPROPERTY()
	FString TrelloFullName;

	/** UTC, ISO 8601. */
	UPROPERTY()
	FString ConnectedAtUtc;

	bool IsSet() const { return Provider != EProtoBoyTaskProvider::None; }
};

/** Loads/saves FProtoBoyTasksConnection. */
class PROTOBOY_API FProtoBoyTasksConnectionStore
{
public:
	static FString GetFilePath();

	/** False (and Out reset) if there is no file or it can't be read. */
	static bool Load(FProtoBoyTasksConnection& Out);

	static bool Save(const FProtoBoyTasksConnection& Connection);

	/** Delete the file. */
	static void Clear();
};
