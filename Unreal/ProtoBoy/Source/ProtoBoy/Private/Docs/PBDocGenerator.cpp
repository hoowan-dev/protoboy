#include "Docs/PBDocGenerator.h"

#include "Docs/Obsidian/PBObsidianConnector.h"
#include "Docs/PBMarkdown.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "PBModule.h"
#include "PBSettings.h"

#define LOCTEXT_NAMESPACE "ProtoBoyDocGenerator"

const TCHAR* FProtoBoyDocGenerator::ArchitectureDocPath = TEXT("Architecture.md");
const TCHAR* FProtoBoyDocGenerator::DesignFolder = TEXT("Design");
const TCHAR* FProtoBoyDocGenerator::ADRFolder = TEXT("ADR");

namespace ProtoBoyDocGenerator
{
	const TCHAR* ArchitectureTemplate = TEXT(R"MD(# Architecture

> The source of truth for how this game is structured. ProtoBoy hands this file to AI agents before they design or build
> anything, so keep it current. Humans own this text: agents only append to it or edit inside `protoboy` managed blocks.

## End Goal

What the finished game is, in a few sentences. Every system and task should serve this.

## Current Milestone

The milestone the team is working toward now, and what "done" means for it.

## Systems

One `###` heading per system. Agents use these to decide where new work belongs. Replace the example.

### Example: Combat

- **Owns:** damage, hit reactions, ability execution
- **Code:** Source/<Module>/Combat/
- **Depends on:** Core, Animation
- **Must not:** reference UI directly (raise events instead)
- **Design doc:** Design/Combat.md

## Dependency Rules

- Lower layers never depend on higher ones (for example Core -> Gameplay -> UI).
- A new system or module needs an ADR (see the ADR folder) before code is written.

## Conventions

- Naming, folder layout, when to use Blueprint vs C++, ...

## Roadmap

- Milestones after the current one.
)MD");

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
		while ((From = Haystack.Find(Needle, ESearchCase::CaseSensitive, ESearchDir::FromStart, From)) != INDEX_NONE)
		{
			++Count;
			From += Needle.Len();
		}
		return Count;
	}

	FString MakeSlug(const FString& Title)
	{
		FString Slug;
		bool bDash = false;
		for (const TCHAR C : Title)
		{
			if (FChar::IsAlnum(C))
			{
				Slug.AppendChar(FChar::ToLower(C));
				bDash = false;
			}
			else if (!bDash && !Slug.IsEmpty())
			{
				Slug.AppendChar(TEXT('-'));
				bDash = true;
			}
		}
		while (Slug.EndsWith(TEXT("-")))
		{
			Slug.LeftChopInline(1);
		}
		return Slug.Left(60).IsEmpty() ? FString(TEXT("decision")) : Slug.Left(60);
	}
}

FProtoBoyDocGenerator::FProtoBoyDocGenerator(const FProtoBoyObsidianConnector& InObsidian)
	: Obsidian(InObsidian)
	, ObsidianDocs(InObsidian)
{
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

bool FProtoBoyDocGenerator::HasArchitectureDoc() const
{
	return GetProvider().DocExists(ArchitectureDocPath);
}

bool FProtoBoyDocGenerator::CreateArchitectureDoc(FText& OutError)
{
	return GetProvider().WriteDoc(ArchitectureDocPath, ProtoBoyDocGenerator::ArchitectureTemplate, /*bMustNotExist*/ true, OutError);
}

FProtoBoyArchitecture FProtoBoyDocGenerator::ReadArchitecture() const
{
	FProtoBoyArchitecture Architecture;
	FString Text;
	FText Ignored;
	if (!GetProvider().ReadDoc(ArchitectureDocPath, Text, Ignored))
	{
		return Architecture;
	}

	Architecture.bExists = true;
	Architecture.EndGoal = ProtoBoyMarkdown::GetSectionBody(Text, TEXT("End Goal"));
	Architecture.CurrentMilestone = ProtoBoyMarkdown::GetSectionBody(Text, TEXT("Current Milestone"));
	Architecture.DependencyRules = ProtoBoyMarkdown::GetSectionBody(Text, TEXT("Dependency Rules"));
	Architecture.Conventions = ProtoBoyMarkdown::GetSectionBody(Text, TEXT("Conventions"));
	Architecture.Roadmap = ProtoBoyMarkdown::GetSectionBody(Text, TEXT("Roadmap"));

	const TArray<ProtoBoyMarkdown::FSection> Sections = ProtoBoyMarkdown::ParseSections(Text);
	if (const ProtoBoyMarkdown::FSection* SystemsSection = ProtoBoyMarkdown::FindSection(Sections, TEXT("Systems")))
	{
		for (const ProtoBoyMarkdown::FSection& Section : Sections)
		{
			const bool bInside = Section.HeadingStart > SystemsSection->HeadingStart && Section.HeadingStart < SystemsSection->End;
			// The template's example system isn't a real one.
			if (bInside && Section.Level == SystemsSection->Level + 1 && !Section.Heading.StartsWith(TEXT("Example"), ESearchCase::IgnoreCase))
			{
				FProtoBoyArchitectureSystem System;
				System.Name = Section.Heading;
				System.Text = Text.Mid(Section.BodyStart, Section.End - Section.BodyStart).TrimStartAndEnd();
				Architecture.Systems.Add(MoveTemp(System));
			}
		}
	}
	return Architecture;
}

bool FProtoBoyDocGenerator::CreateADR(const FString& Title, const FString& Context, const FString& Decision, const FString& Consequences,
	const FString& Status, const FString& ProposedBy, FString& OutPath, FText& OutError)
{
	if (Title.TrimStartAndEnd().IsEmpty())
	{
		OutError = LOCTEXT("NoTitle", "An ADR needs a title.");
		return false;
	}

	// Next number after the highest existing ADR-NNNN.
	int32 Highest = 0;
	const FString Prefix = FString(ADRFolder) + TEXT("/ADR-");
	for (const FProtoBoyDocInfo& Doc : GetProvider().ListDocs())
	{
		if (Doc.Path.StartsWith(Prefix, ESearchCase::IgnoreCase))
		{
			Highest = FMath::Max(Highest, FCString::Atoi(*Doc.Path.Mid(Prefix.Len(), 4)));
		}
	}
	const int32 Number = Highest + 1;

	OutPath = FString::Printf(TEXT("%s/ADR-%04d-%s.md"), ADRFolder, Number, *ProtoBoyDocGenerator::MakeSlug(Title));

	auto OrPlaceholder = [](const FString& Value) { return Value.TrimStartAndEnd().IsEmpty() ? FString(TEXT("_To be filled in._")) : Value.TrimStartAndEnd(); };

	const FString Text = FString::Printf(TEXT(
		"# ADR-%04d: %s\n"
		"\n"
		"- **Status:** %s\n"
		"- **Date:** %s\n"
		"- **Proposed by:** %s\n"
		"\n"
		"## Context\n\n%s\n"
		"\n"
		"## Decision\n\n%s\n"
		"\n"
		"## Consequences\n\n%s\n"),
		Number, *Title.TrimStartAndEnd(),
		Status.TrimStartAndEnd().IsEmpty() ? TEXT("Proposed") : *Status.TrimStartAndEnd(),
		*FDateTime::UtcNow().ToString(TEXT("%Y-%m-%d")),
		ProposedBy.IsEmpty() ? TEXT("unknown") : *ProposedBy,
		*OrPlaceholder(Context), *OrPlaceholder(Decision), *OrPlaceholder(Consequences));

	return GetProvider().WriteDoc(OutPath, Text, /*bMustNotExist*/ true, OutError);
}

FString FProtoBoyDocGenerator::GenerateTDDStub(const FSoftObjectPath& AssetPath)
{
	// TODO: Extract class/variables/functions and write <DocsRoot>/TDD/<Asset>.md,
	//       regenerating only the auto-generated block so human edits survive.
	UE_LOG(LogProtoBoy, Verbose, TEXT("GenerateTDDStub not implemented (%s)."), *AssetPath.ToString());
	return FString();
}

#undef LOCTEXT_NAMESPACE
