#include "Sourcing/PBAssetSourcing.h"

#include "Docs/PBDocGenerator.h"
#include "PBSettings.h"
#include "Sourcing/Fab/PBFabSource.h"

namespace ProtoBoyAssetSourcing
{
	FString GetArtDirectionDocPath()
	{
		const FString& Configured = GetDefault<UProtoBoySettings>()->ArtDirectionDocPath;
		FString Normalized;
		FText Ignored;
		return !Configured.TrimStartAndEnd().IsEmpty() && FProtoBoyDocGenerator::NormalizeDocPath(Configured, Normalized, Ignored)
			? Normalized
			: FString();
	}

	FString DescribePolicyForAgents()
	{
		const UProtoBoySettings* Settings = GetDefault<UProtoBoySettings>();

		FString Out;
		if (Settings->bPreferSourcedAssets)
		{
			Out += TEXT("- **Policy:** don't generate art, audio or other assets with AI. Use the sources below, or clearly "
				"marked placeholders (greybox shapes, temp audio), and ask the user before anything else.\n");
		}
		else
		{
			Out += TEXT("- **Policy:** no restriction set. Still prefer the sources below where they fit.\n");
		}

		TArray<FString> Sources;
		if (ProtoBoyFab::IsEnabled())
		{
			Sources.Add(TEXT("Fab (fab_search: opens a search in the user's browser; the user picks listings)"));
		}
		Out += FString::Printf(TEXT("- **Sources:** %s\n"), Sources.IsEmpty() ? TEXT("none enabled (ask the user where assets should come from)") : *FString::Join(Sources, TEXT("; ")));

		const FString ArtDoc = GetArtDirectionDocPath();
		Out += ArtDoc.IsEmpty()
			? FString(TEXT("- **Art direction:** no doc is set; search the docs (docs_search \"art style\") or ask the user before choosing assets.\n"))
			: FString::Printf(TEXT("- **Art direction:** read %s (docs_read) before choosing assets; matches must fit its style and goals.\n"), *ArtDoc);
		return Out;
	}
}
