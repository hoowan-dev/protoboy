#include "Agents/PBAgentPromptContext.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Docs/PBDocMap.h"
#include "Docs/PBDocsProvider.h"
#include "Docs/PBMarkdown.h"
#include "Misc/PackageName.h"
#include "PBSubsystem.h"

namespace ProtoBoyAgentPrompt
{
	/** Per list in DescribeProjectAssets. */
	constexpr int32 MaxListed = 150;

	FString DescribeProjectAssets()
	{
		TArray<FAssetData> Assets;
		IAssetRegistry::GetChecked().GetAssetsByPath(TEXT("/Game"), Assets, /*bRecursive*/ true);
		if (Assets.IsEmpty())
		{
			return TEXT("_The Content folder is empty._\n");
		}
		Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.PackageName.LexicalLess(B.PackageName); });

		TMap<FString, int32> ByClass;
		TMap<FString, int32> ByFolder;
		TArray<FString> Maps;
		TArray<FString> Blueprints;
		TArray<FString> Data;
		for (const FAssetData& Asset : Assets)
		{
			const FString Class = Asset.AssetClassPath.GetAssetName().ToString();
			const FString Path = Asset.PackageName.ToString();
			++ByClass.FindOrAdd(Class);

			// Top two folder levels below /Game, e.g. /Game/Characters/Hero.
			TArray<FString> Parts;
			Path.ParseIntoArray(Parts, TEXT("/"));
			if (Parts.Num() > 2)
			{
				++ByFolder.FindOrAdd(TEXT("/") + Parts[0] + TEXT("/") + Parts[1] + (Parts.Num() > 3 ? TEXT("/") + Parts[2] : FString()));
			}

			if (Class == TEXT("World"))
			{
				Maps.Add(Path);
			}
			else if (Class.EndsWith(TEXT("Blueprint")) || Class == TEXT("Blueprint"))
			{
				FString Parent = Asset.GetTagValueRef<FString>(FName(TEXT("ParentClass")));
				Parent = FPackageName::ObjectPathToObjectName(Parent);
				Parent.RemoveFromEnd(TEXT("'"));
				Blueprints.Add(Parent.IsEmpty() ? Path : FString::Printf(TEXT("%s (%s, parent %s)"), *Path, *Class, *Parent));
			}
			else if (Class == TEXT("DataTable") || Class.Contains(TEXT("DataAsset")) || Class == TEXT("CurveTable") || Class.EndsWith(TEXT("Data")))
			{
				Data.Add(FString::Printf(TEXT("%s (%s)"), *Path, *Class));
			}
		}

		auto List = [](const TArray<FString>& Items) -> FString
		{
			FString Out;
			for (int32 Index = 0; Index < Items.Num() && Index < MaxListed; ++Index)
			{
				Out += FString::Printf(TEXT("- %s\n"), *Items[Index]);
			}
			if (Items.Num() > MaxListed)
			{
				Out += FString::Printf(TEXT("- ...and %d more\n"), Items.Num() - MaxListed);
			}
			return Out.IsEmpty() ? FString(TEXT("_None._\n")) : Out;
		};
		auto Counts = [](const TMap<FString, int32>& Map) -> FString
		{
			TArray<TPair<FString, int32>> Sorted;
			for (const TPair<FString, int32>& Pair : Map)
			{
				Sorted.Add(Pair);
			}
			Sorted.Sort([](const TPair<FString, int32>& A, const TPair<FString, int32>& B) { return A.Value > B.Value; });
			FString Out;
			for (int32 Index = 0; Index < Sorted.Num() && Index < 60; ++Index)
			{
				Out += FString::Printf(TEXT("- %s: %d\n"), *Sorted[Index].Key, Sorted[Index].Value);
			}
			return Out;
		};

		FString Out = FString::Printf(TEXT("%d assets.\n\n### By type\n\n%s\n### By folder\n\n%s\n"), Assets.Num(), *Counts(ByClass), *Counts(ByFolder));
		Out += TEXT("### Maps\n\n") + List(Maps);
		Out += TEXT("\n### Blueprints\n\n") + List(Blueprints);
		Out += TEXT("\n### Data assets and tables\n\n") + List(Data);
		return Out;
	}

	FString ReadDoc(const FProtoBoyDocNode& Doc, int32 MaxChars)
	{
		const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
		FString Text;
		FText Error;
		if (!Subsystem || !Subsystem->GetDocsProvider().ReadDoc(Doc.Path, Text, Error))
		{
			return FString::Printf(TEXT("_Couldn't read this doc: %s_\n"), *Error.ToString());
		}
		const ProtoBoyMarkdown::FFrontmatter Frontmatter = ProtoBoyMarkdown::ParseFrontmatter(Text);
		Text = ProtoBoyMarkdown::StripComments(Text.Mid(Frontmatter.BodyStart)).TrimStartAndEnd();
		if (Text.Len() > MaxChars)
		{
			Text = Text.Left(MaxChars) + TEXT("\n\n_(cut off here)_");
		}
		return Text + TEXT("\n");
	}
}
