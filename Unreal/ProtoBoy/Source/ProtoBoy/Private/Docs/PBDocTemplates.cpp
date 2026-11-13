#include "Docs/PBDocTemplates.h"

#include "Docs/PBDocLayers.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PBModule.h"

namespace ProtoBoyDocTemplates
{
	FString GetTemplatesDir()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ProtoBoy"));
		return Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources"), TEXT("DocTemplates")) : FString();
	}

	/** Why a template can't be used, or empty. */
	FString Validate(const FProtoBoyDocTemplate& Template)
	{
		TSet<FString> Seen;
		for (const FProtoBoyDocTemplateDoc& Doc : Template.Docs)
		{
			EProtoBoyDocKind Kind;
			if (Doc.Id.IsEmpty() || Doc.Title.TrimStartAndEnd().IsEmpty())
			{
				return TEXT("a doc has no id or title");
			}
			if (!ProtoBoyDocLayers::FromTemplateKey(Doc.Kind, Kind))
			{
				return FString::Printf(TEXT("doc \"%s\" has kind \"%s\" (expected design, tech, content or style)"), *Doc.Id, *Doc.Kind);
			}
			if (!Doc.Parent.IsEmpty() && !Seen.Contains(Doc.Parent))
			{
				return FString::Printf(TEXT("doc \"%s\" comes before its parent \"%s\" (or the parent is missing)"), *Doc.Id, *Doc.Parent);
			}
			if (Seen.Contains(Doc.Id))
			{
				return FString::Printf(TEXT("two docs have the id \"%s\""), *Doc.Id);
			}
			Seen.Add(Doc.Id);
		}
		return FString();
	}

	TArray<FProtoBoyDocTemplate> LoadTemplates()
	{
		TArray<FProtoBoyDocTemplate> Templates;
		const FString Dir = GetTemplatesDir();
		if (Dir.IsEmpty())
		{
			return Templates;
		}

		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *FPaths::Combine(Dir, TEXT("*.json")), true, false);
		for (const FString& File : Files)
		{
			const FString Path = FPaths::Combine(Dir, File);
			FString Json;
			FProtoBoyDocTemplate Template;
			if (!FFileHelper::LoadFileToString(Json, *Path) || !FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Template))
			{
				UE_LOG(LogProtoBoy, Warning, TEXT("Skipping doc template %s: couldn't read it."), *Path);
				continue;
			}
			if (Template.Version > FProtoBoyDocTemplate::CurrentVersion)
			{
				UE_LOG(LogProtoBoy, Warning, TEXT("Skipping doc template %s: written by a newer ProtoBoy (version %d)."), *Path, Template.Version);
				continue;
			}
			if (const FString Problem = Validate(Template); !Problem.IsEmpty())
			{
				UE_LOG(LogProtoBoy, Warning, TEXT("Skipping doc template %s: %s."), *Path, *Problem);
				continue;
			}
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

		Templates.StableSort([](const FProtoBoyDocTemplate& A, const FProtoBoyDocTemplate& B)
		{
			return A.SortOrder != B.SortOrder ? A.SortOrder < B.SortOrder : A.Name < B.Name;
		});
		return Templates;
	}
}
