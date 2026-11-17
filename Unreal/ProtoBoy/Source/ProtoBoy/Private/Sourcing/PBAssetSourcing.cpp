#include "Sourcing/PBAssetSourcing.h"

#include "PBSettings.h"
#include "Sourcing/Fab/PBFabSource.h"

#define LOCTEXT_NAMESPACE "ProtoBoyAssetSourcing"

namespace ProtoBoyAssetSourcing
{
	const TArray<FProtoBoyAssetSource>& GetSources()
	{
		static const TArray<FProtoBoyAssetSource> Sources = {
			{
				LOCTEXT("FabName", "Fab"),
				LOCTEXT("FabDesc", "Epic's marketplace for Unreal assets: 3D models, materials, audio, VFX and more, free and paid. Searches open in your browser and you pick the listings."),
				"ProtoBoy.Icon.Fab",
				&ProtoBoyFab::IsEnabled,
				[](const FProtoBoyAssetSearch& Search, FString& OutUrl, FText& OutError)
				{
					return ProtoBoyFab::OpenSearch(ProtoBoyFab::MakeSearch(Search), OutUrl, OutError);
				}
			},
		};
		return Sources;
	}

	bool HasEnabledSource()
	{
		return GetSources().ContainsByPredicate([](const FProtoBoyAssetSource& Source) { return Source.IsEnabled(); });
	}

	FText GetAssetTypeName(EProtoBoyAssetType Type)
	{
		switch (Type)
		{
		case EProtoBoyAssetType::Model3D:         return LOCTEXT("Type3D", "3D");
		case EProtoBoyAssetType::MaterialTexture: return LOCTEXT("TypeMaterial", "Materials and Textures");
		case EProtoBoyAssetType::SpriteFlipbook:  return LOCTEXT("TypeSprite", "Sprites and Flipbooks");
		case EProtoBoyAssetType::Decal:           return LOCTEXT("TypeDecal", "Decals");
		case EProtoBoyAssetType::Brush:           return LOCTEXT("TypeBrush", "Brushes");
		case EProtoBoyAssetType::HDRI:            return LOCTEXT("TypeHDRI", "HDRI");
		case EProtoBoyAssetType::Animation:       return LOCTEXT("TypeAnimation", "Animations");
		case EProtoBoyAssetType::Audio:           return LOCTEXT("TypeAudio", "Audio");
		case EProtoBoyAssetType::VFX:             return LOCTEXT("TypeVFX", "VFX");
		case EProtoBoyAssetType::UI:              return LOCTEXT("TypeUI", "UI");
		case EProtoBoyAssetType::GameSystem:      return LOCTEXT("TypeGameSystem", "Game Systems");
		case EProtoBoyAssetType::GameTemplate:    return LOCTEXT("TypeGameTemplate", "Game Templates");
		case EProtoBoyAssetType::ToolPlugin:      return LOCTEXT("TypeToolPlugin", "Tools and Plugins");
		case EProtoBoyAssetType::Tutorial:        return LOCTEXT("TypeTutorial", "Tutorials and Examples");
		default:                                  return FText::GetEmpty();
		}
	}

	FString GetAssetTypeKey(EProtoBoyAssetType Type)
	{
		switch (Type)
		{
		case EProtoBoyAssetType::Model3D:         return TEXT("model-3d");
		case EProtoBoyAssetType::MaterialTexture: return TEXT("material-texture");
		case EProtoBoyAssetType::SpriteFlipbook:  return TEXT("sprite-flipbook");
		case EProtoBoyAssetType::Decal:           return TEXT("decal");
		case EProtoBoyAssetType::Brush:           return TEXT("brush");
		case EProtoBoyAssetType::HDRI:            return TEXT("hdri");
		case EProtoBoyAssetType::Animation:       return TEXT("animation");
		case EProtoBoyAssetType::Audio:           return TEXT("audio");
		case EProtoBoyAssetType::VFX:             return TEXT("vfx");
		case EProtoBoyAssetType::UI:              return TEXT("ui");
		case EProtoBoyAssetType::GameSystem:      return TEXT("game-system");
		case EProtoBoyAssetType::GameTemplate:    return TEXT("game-template");
		case EProtoBoyAssetType::ToolPlugin:      return TEXT("tool-plugin");
		case EProtoBoyAssetType::Tutorial:        return TEXT("tutorial");
		default:                                  return FString();
		}
	}

	bool FromAssetTypeKey(const FString& Key, EProtoBoyAssetType& OutType)
	{
		const FString Trimmed = Key.TrimStartAndEnd();
		for (int32 Index = 0; Index < static_cast<int32>(EProtoBoyAssetType::Count); ++Index)
		{
			const EProtoBoyAssetType Type = static_cast<EProtoBoyAssetType>(Index);
			if (Trimmed.Equals(GetAssetTypeKey(Type), ESearchCase::IgnoreCase))
			{
				OutType = Type;
				return true;
			}
		}
		return false;
	}

	int32 OpenSearch(const FProtoBoyAssetSearch& Search, TArray<FText>& OutErrors)
	{
		int32 Opened = 0;
		for (const FProtoBoyAssetSource& Source : GetSources())
		{
			if (!Source.IsEnabled())
			{
				continue;
			}
			FString Url;
			FText Error;
			if (Source.OpenSearch(Search, Url, Error))
			{
				++Opened;
			}
			else
			{
				OutErrors.Add(FText::Format(LOCTEXT("SourceError", "{0}: {1}"), Source.Name, Error));
			}
		}
		return Opened;
	}

	TArray<FText> GetMissingSetup()
	{
		TArray<FText> Missing;
		if (!GetDefault<UProtoBoySettings>()->bPreferSourcedAssets)
		{
			Missing.Add(LOCTEXT("MissingPreferSourced", "suggesting existing assets instead of generating them with AI"));
		}
		if (!HasEnabledSource())
		{
			Missing.Add(LOCTEXT("MissingSource", "at least one asset source (like Fab)"));
		}
		return Missing;
	}

	FString DescribePolicyForAgents()
	{
		const UProtoBoySettings* Settings = GetDefault<UProtoBoySettings>();

		FString Out;
		if (Settings->bPreferSourcedAssets)
		{
			Out += TEXT("- **Policy:** don't generate art, audio or other assets with AI. Suggest paid and free assets from the "
				"sources below (say which are paid), or clearly marked placeholders (greybox shapes, temp audio), and ask the user before anything else.\n");
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
			"out. Also look at the project's existing assets (folders and names under Content/) so suggestions match what's "
			"already there and don't duplicate it. If no Content doc covers the need, ask the user what style to look for.\n");
		return Out;
	}
}

#undef LOCTEXT_NAMESPACE
