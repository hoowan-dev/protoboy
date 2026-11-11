#pragma once

#include "CoreMinimal.h"

enum class EProtoBoyIssueSeverity : uint8
{
	Info,
	Warning,
	Error,
};

struct FProtoBoyHealthIssue
{
	FName CheckId;
	EProtoBoyIssueSeverity Severity = EProtoBoyIssueSeverity::Info;
	FText Message;
	/** Asset path or source file the issue refers to. */
	FString Location;
};

/** Pillar 4: project health checks & anti-slop auditing. */
class PROTOBOY_API FProtoBoyHealthChecker
{
public:
	/** Run every check and cache the results. */
	void RunAllChecks();

	const TArray<FProtoBoyHealthIssue>& GetIssues() const { return Issues; }

	/** Prototype-to-Production Viability Index, 0-100. */
	int32 ComputeViabilityIndex() const;

private:
	// TODO: Also god classes, magic numbers, circular deps, TODO/HACK markers.
	void CheckBlueprintTickUsage();
	void CheckAssetBudgets();
	void CheckOrphanAssets();

	TArray<FProtoBoyHealthIssue> Issues;
};
