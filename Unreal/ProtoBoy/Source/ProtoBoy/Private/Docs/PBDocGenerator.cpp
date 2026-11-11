#include "Docs/PBDocGenerator.h"

#include "Misc/Paths.h"
#include "PBModule.h"
#include "PBSettings.h"

FString FProtoBoyDocGenerator::GetDocsRoot() const
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), GetDefault<UProtoBoySettings>()->DocsDirectory);
}

FString FProtoBoyDocGenerator::GenerateTDDStub(const FSoftObjectPath& AssetPath)
{
	// TODO: Extract class/variables/functions and write <DocsRoot>/TDD/<Asset>.md,
	//       regenerating only the auto-generated block so human edits survive.
	UE_LOG(LogProtoBoy, Verbose, TEXT("GenerateTDDStub not implemented (%s)."), *AssetPath.ToString());
	return FString();
}

FString FProtoBoyDocGenerator::CreateADR(const FString& Title, const FString& Why)
{
	// TODO: Write <DocsRoot>/ADR/ADR-NNNN-<slug>.md using the next free number.
	UE_LOG(LogProtoBoy, Verbose, TEXT("CreateADR not implemented (%s)."), *Title);
	return FString();
}
