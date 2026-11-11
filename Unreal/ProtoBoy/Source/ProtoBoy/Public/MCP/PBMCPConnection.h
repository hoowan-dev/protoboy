#pragma once

#include "CoreMinimal.h"
#include "PBMCPConnection.generated.h"

/** LLM providers shown in the "Connect LLM Provider" window. Each groups one or more MCP clients. */
UENUM()
enum class EProtoBoyLLMProvider : uint8
{
	Claude,
	Gemini,
};

/** Agent applications that can connect to ProtoBoy's MCP server. */
UENUM()
enum class EProtoBoyMCPClient : uint8
{
	ClaudeCode,
	ClaudeDesktop,
	GeminiCLI,
};

/**
 * This user's MCP state, serialized as JSON to <Project>/Saved/ProtoBoy/MCPConnection.json
 * (per user, never committed: it holds the auth token).
 * Add fields freely: unknown/missing fields are ignored on load. Bump Version for breaking changes.
 */
USTRUCT()
struct PROTOBOY_API FProtoBoyMCPConnection
{
	GENERATED_BODY()

	static constexpr int32 CurrentVersion = 1;

	UPROPERTY()
	int32 Version = CurrentVersion;

	/** Bearer token clients must send. Generated on first use. */
	UPROPERTY()
	FString AuthToken;

	/** Clients this user has written a ProtoBoy entry into (via "Write Config"). */
	UPROPERTY()
	TArray<EProtoBoyMCPClient> ConfiguredClients;
};

/** Loads/saves FProtoBoyMCPConnection. */
class PROTOBOY_API FProtoBoyMCPConnectionStore
{
public:
	static FString GetFilePath();

	/** False (and Out reset) if there is no file or it can't be read. */
	static bool Load(FProtoBoyMCPConnection& Out);

	static bool Save(const FProtoBoyMCPConnection& Connection);
};
