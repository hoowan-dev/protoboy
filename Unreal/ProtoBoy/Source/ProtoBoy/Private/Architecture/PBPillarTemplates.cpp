#include "Architecture/PBPillarTemplates.h"

#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "JsonObjectConverter.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PBModule.h"

namespace ProtoBoyPillarTemplates
{
	FString GetTemplatesDir()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ProtoBoy"));
		return Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources"), TEXT("PillarTemplates")) : FString();
	}

	TArray<FProtoBoyPillarTemplate> LoadTemplates()
	{
		TArray<FProtoBoyPillarTemplate> Templates;
		const FString Dir = GetTemplatesDir();
		if (Dir.IsEmpty())
		{
			return Templates;
		}

		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *FPaths::Combine(Dir, TEXT("*.json")), true, false);

		const FString ProjectName = FApp::GetProjectName();
		for (const FString& File : Files)
		{
			const FString Path = FPaths::Combine(Dir, File);
			FString Json;
			FProtoBoyPillarTemplate Template;
			if (!FFileHelper::LoadFileToString(Json, *Path) || !FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Template))
			{
				UE_LOG(LogProtoBoy, Warning, TEXT("Skipping pillar template %s: couldn't read it."), *Path);
				continue;
			}
			if (Template.Version > FProtoBoyPillarTemplate::CurrentVersion)
			{
				UE_LOG(LogProtoBoy, Warning, TEXT("Skipping pillar template %s: written by a newer ProtoBoy (version %d)."), *Path, Template.Version);
				continue;
			}
			if (Template.Pillars.IsEmpty() || Template.Pillars[0].Type != EProtoBoyPillarType::Game)
			{
				UE_LOG(LogProtoBoy, Warning, TEXT("Skipping pillar template %s: its first pillar must be the root, with type Game."), *Path);
				continue;
			}
			FProtoBoyPillar* Root = &Template.Pillars[0];

			// The root stands for the game, so it shows (and is applied with) this project's name.
			Root->Name = ProjectName.IsEmpty() ? FString(TEXT("Game")) : ProjectName;
			if (Template.Id.IsEmpty())
			{
				Template.Id = FPaths::GetBaseFilename(File).ToLower();
			}
			if (Template.Name.IsEmpty())
			{
				Template.Name = FPaths::GetBaseFilename(File);
			}
			Templates.Add(MoveTemp(Template));
		}

		Templates.StableSort([](const FProtoBoyPillarTemplate& A, const FProtoBoyPillarTemplate& B)
		{
			return A.SortOrder != B.SortOrder ? A.SortOrder < B.SortOrder : A.Name < B.Name;
		});
		return Templates;
	}
}
