#include "Health/ProtoBoyHealthChecker.h"

void FProtoBoyHealthChecker::RunAllChecks()
{
	Issues.Reset();
	CheckBlueprintTickUsage();
	CheckAssetBudgets();
	CheckOrphanAssets();
}

int32 FProtoBoyHealthChecker::ComputeViabilityIndex() const
{
	// TODO: Weight modularity, decoupling, test coverage and doc freshness.
	return 100;
}

void FProtoBoyHealthChecker::CheckBlueprintTickUsage()
{
	// TODO
}

void FProtoBoyHealthChecker::CheckAssetBudgets()
{
	// TODO: Compare asset registry tags ("Dimensions", "Triangles") against UProtoBoySettings budgets.
}

void FProtoBoyHealthChecker::CheckOrphanAssets()
{
	// TODO
}
