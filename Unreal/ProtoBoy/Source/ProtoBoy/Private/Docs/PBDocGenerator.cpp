#include "Docs/PBDocGenerator.h"

#include "Docs/Obsidian/PBObsidianConnector.h"
#include "Docs/PBMarkdown.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "PBModule.h"
#include "PBSettings.h"

#define LOCTEXT_NAMESPACE "ProtoBoyDocGenerator"


namespace ProtoBoyDocGenerator
{
	/** Lowercase words of 2+ letters/digits. */
	TArray<FString> Tokenize(const FString& Text)
	{
		TArray<FString> Words;
		FString Current;
		for (const TCHAR C : Text)
		{
			if (FChar::IsAlnum(C))
			{
				Current.AppendChar(FChar::ToLower(C));
			}
			else
			{
				if (Current.Len() >= 2)
				{
					Words.AddUnique(Current);
				}
				Current.Reset();
			}
		}
		if (Current.Len() >= 2)
		{
			Words.AddUnique(Current);
		}
		return Words;
	}

	int32 CountOccurrences(const FString& Haystack, const FString& Needle)
	{
		int32 Count = 0;
		int32 From = 0;
		// FString::Find clamps From to the last character, so stop at the end rather than search from past it.
		while (From < Haystack.Len() && (From = Haystack.Find(Needle, ESearchCase::CaseSensitive, ESearchDir::FromStart, From)) != INDEX_NONE)
		{
			++Count;
			From += Needle.Len();
		}
		return Count;
	}

}

FProtoBoyDocGenerator::FProtoBoyDocGenerator(const FProtoBoyObsidianConnector& InObsidian)
	: Obsidian(InObsidian)
	, ObsidianDocs(InObsidian)
{
}

bool FProtoBoyDocGenerator::NormalizeDocPath(const FString& Path, FString& OutNormalized, FText& OutError)
{
	FString Out = Path.TrimStartAndEnd();
	Out.ReplaceInline(TEXT("\\"), TEXT("/"));
	while (Out.StartsWith(TEXT("/")))
	{
		Out.RightChopInline(1);
	}
	if (Out.IsEmpty())
	{
		OutError = LOCTEXT("EmptyPath", "Enter a path, e.g. \"Design/Combat.md\".");
		return false;
	}
	if (Out.Contains(TEXT(":")) || Out.Contains(TEXT("..")) || Out.Contains(TEXT("//")))
	{
		OutError = LOCTEXT("UnsafePath", "The path must be inside the docs folder (no drive letters or \"..\").");
		return false;
	}
	for (const TCHAR C : FString(TEXT("<>\"|?*")))
	{
		if (Out.Contains(FString::Chr(C)))
		{
			OutError = LOCTEXT("BadChars", "The path can't contain < > \" | ? *");
			return false;
		}
	}
	if (!Out.EndsWith(TEXT(".md"), ESearchCase::IgnoreCase))
	{
		Out += TEXT(".md");
	}
	OutNormalized = Out;
	return true;
}

IProtoBoyDocsProvider& FProtoBoyDocGenerator::GetProvider() const
{
	if (Obsidian.IsConnected())
	{
		return ObsidianDocs;
	}
	return LocalDocs;
}

FString FProtoBoyDocGenerator::GetDocsRoot() const
{
	return Obsidian.IsConnected() ? ObsidianDocs.GetDocsRoot() : LocalDocs.GetDocsRoot();
}

TArray<FProtoBoyDocSearchHit> FProtoBoyDocGenerator::Search(const FString& Query, int32 MaxResults) const
{
	TArray<FProtoBoyDocSearchHit> Hits;
	const TArray<FString> Words = ProtoBoyDocGenerator::Tokenize(Query);
	if (Words.IsEmpty())
	{
		return Hits;
	}

	const IProtoBoyDocsProvider& Provider = GetProvider();
	for (const FProtoBoyDocInfo& Doc : Provider.ListDocs())
	{
		FString Text;
		FText Ignored;
		if (!Provider.ReadDoc(Doc.Path, Text, Ignored))
		{
			continue;
		}

		const FString LowerText = Text.ToLower();
		const FString LowerTitle = (Doc.Title + TEXT(" ") + Doc.Path).ToLower();

		int32 Score = 0;
		int32 FirstMatch = INDEX_NONE;
		for (const FString& Word : Words)
		{
			Score += 5 * ProtoBoyDocGenerator::CountOccurrences(LowerTitle, Word);
			Score += ProtoBoyDocGenerator::CountOccurrences(LowerText, Word);
			const int32 At = LowerText.Find(Word, ESearchCase::CaseSensitive);
			if (At != INDEX_NONE && (FirstMatch == INDEX_NONE || At < FirstMatch))
			{
				FirstMatch = At;
			}
		}
		if (Score == 0)
		{
			continue;
		}

		FProtoBoyDocSearchHit Hit;
		Hit.Path = Doc.Path;
		Hit.Title = Doc.Title;
		Hit.Score = Score;
		const int32 SnippetStart = FMath::Max(0, (FirstMatch == INDEX_NONE ? 0 : FirstMatch) - 80);
		Hit.Snippet = Text.Mid(SnippetStart, 240).Replace(TEXT("\r"), TEXT("")).Replace(TEXT("\n"), TEXT(" ")).TrimStartAndEnd();
		Hits.Add(MoveTemp(Hit));
	}

	Hits.Sort([](const FProtoBoyDocSearchHit& A, const FProtoBoyDocSearchHit& B) { return A.Score > B.Score; });
	if (Hits.Num() > MaxResults)
	{
		Hits.SetNum(MaxResults);
	}
	return Hits;
}

bool FProtoBoyDocGenerator::CreateDoc(const FString& Path, const FString& Text, FText& OutError)
{
	return GetProvider().WriteDoc(Path, Text, /*bMustNotExist*/ true, OutError);
}

bool FProtoBoyDocGenerator::AppendToSection(const FString& Path, const FString& Heading, const FString& Content, FText& OutError)
{
	IProtoBoyDocsProvider& Provider = GetProvider();
	FString Text;
	if (Provider.DocExists(Path))
	{
		if (!Provider.ReadDoc(Path, Text, OutError))
		{
			return false;
		}
	}
	else
	{
		Text = FString::Printf(TEXT("# %s\n"), *FPaths::GetBaseFilename(Path));
	}

	return Provider.WriteDoc(Path, ProtoBoyMarkdown::AppendToSection(Text, Heading, Content), false, OutError);
}

bool FProtoBoyDocGenerator::SetManagedBlock(const FString& Path, const FString& Heading, const FString& BlockId, const FString& Content, FText& OutError)
{
	if (!ProtoBoyMarkdown::IsValidBlockId(BlockId))
	{
		OutError = LOCTEXT("BadBlockId", "Block ids use lowercase letters, digits and '-' (max 64).");
		return false;
	}

	IProtoBoyDocsProvider& Provider = GetProvider();
	FString Text;
	if (Provider.DocExists(Path))
	{
		if (!Provider.ReadDoc(Path, Text, OutError))
		{
			return false;
		}
	}
	else
	{
		Text = FString::Printf(TEXT("# %s\n"), *FPaths::GetBaseFilename(Path));
	}

	return Provider.WriteDoc(Path, ProtoBoyMarkdown::SetManagedBlock(Text, Heading, BlockId, Content), false, OutError);
}

FString FProtoBoyDocGenerator::GenerateTDDStub(const FSoftObjectPath& AssetPath)
{
	// TODO: Extract class/variables/functions and write <DocsRoot>/TDD/<Asset>.md,
	//       regenerating only the auto-generated block so human edits survive.
	UE_LOG(LogProtoBoy, Verbose, TEXT("GenerateTDDStub not implemented (%s)."), *AssetPath.ToString());
	return FString();
}

#undef LOCTEXT_NAMESPACE
