#include "Sourcing/PBAssetSourcing.h"

#include "PBSettings.h"
#include "Sourcing/Fab/PBFabSource.h"

namespace ProtoBoyAssetSourcing
{
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

		Out += TEXT("- **Style:** Content docs (layer content on the doc map) say what assets are needed and the style they must "
			"have. Before searching or choosing assets, read the Content doc for that need (docs_read), and the Content docs above "
			"it on the map: build searches from its What to Look For section, and skip anything its What to Avoid section rules "
			"out. If no Content doc covers the need, ask the user what style to look for.\n");
		return Out;
	}
}
