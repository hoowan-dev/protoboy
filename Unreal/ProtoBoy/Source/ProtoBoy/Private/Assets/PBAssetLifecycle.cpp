#include "Assets/PBAssetLifecycle.h"

EProtoBoyAssetLifecycle FProtoBoyAssetLifecycle::GetLifecycle(const FSoftObjectPath& AssetPath) const
{
	const EProtoBoyAssetLifecycle* Found = Entries.Find(AssetPath);
	return Found ? *Found : EProtoBoyAssetLifecycle::Untracked;
}

void FProtoBoyAssetLifecycle::SetLifecycle(const FSoftObjectPath& AssetPath, EProtoBoyAssetLifecycle Lifecycle)
{
	Entries.Add(AssetPath, Lifecycle);
	// TODO: Save manifest.
}

FString FProtoBoyAssetLifecycle::WriteReplacementMatrix() const
{
	// TODO
	return FString();
}
