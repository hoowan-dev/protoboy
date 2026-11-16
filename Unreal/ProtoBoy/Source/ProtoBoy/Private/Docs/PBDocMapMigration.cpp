// Docs from older versions: merging an old Vision and Overview into one Game doc. Part of FProtoBoyDocMap.
#include "Docs/PBDocMap.h"

#include "Docs/PBDocsService.h"
#include "Docs/PBDocMapUtils.h"
#include "Docs/PBDocTemplates.h"
#include "Docs/PBMarkdown.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "PBModule.h"

#define LOCTEXT_NAMESPACE "ProtoBoyDocMapMigration"

bool FProtoBoyDocMap::CombineGameDocs(FText& OutError)
{
	const TArray<const FProtoBoyDocNode*> Games = GetGameDocs();
	if (Games.Num() < 2)
	{
		return true;
	}
	IProtoBoyDocsProvider& Provider = Docs.GetProvider();

	// Read every Game doc's "## " sections, the root's first.
	struct FDocSections
	{
		FString Path;
		FString Id;
		FString Frontmatter;
		TArray<TPair<FString, FString>> Sections;
	};
	TArray<FDocSections> Read;
	for (const FProtoBoyDocNode* Game : Games)
	{
		FString Text;
		if (!Provider.ReadDoc(Game->Path, Text, OutError))
		{
			return false;
		}
		FDocSections& Entry = Read.AddDefaulted_GetRef();
		Entry.Path = Game->Path;
		Entry.Id = Game->Id;
		Entry.Frontmatter = Text.Left(ProtoBoyMarkdown::ParseFrontmatter(Text).BodyStart);
		for (const ProtoBoyMarkdown::FSection& Section : ProtoBoyMarkdown::ParseSections(Text))
		{
			if (Section.Level == 2)
			{
				Entry.Sections.Emplace(Section.Heading, Text.Mid(Section.BodyStart, Section.End - Section.BodyStart).TrimStartAndEnd());
			}
		}
	}

	// The Game template's sections in order: the first written version wins (hints only if nobody wrote anything).
	TSet<FString> Used;
	FString Body = FString::Printf(TEXT("# %s\n\n> %s\n"), *GetProjectTitle(), *ProtoBoyDocLayers::GetIntro(EProtoBoyDocKind::Game));
	auto Matches = [](const FString& A, const FString& B) { return A.Equals(B, ESearchCase::IgnoreCase); };
	for (const ProtoBoyDocLayers::FHeading& Heading : ProtoBoyDocLayers::GetHeadings(EProtoBoyDocKind::Game))
	{
		Used.Add(Heading.Title.ToLower());
		const bool bDecisions = Matches(Heading.Title, ProtoBoyDocLayers::GetDecisionsHeadingTitle());
		TArray<FString> Written;
		FString Fallback;
		for (const FDocSections& Doc : Read)
		{
			for (const TPair<FString, FString>& Section : Doc.Sections)
			{
				if (!Matches(Section.Key, Heading.Title))
				{
					continue;
				}
				// The Areas index is regenerated; its old copies are dropped.
				const bool bHasWriting = !ProtoBoyDocMap::Clean(Section.Value).IsEmpty() && !Matches(Heading.Title, TEXT("Areas"));
				if (bHasWriting)
				{
					Written.Add(Section.Value);
				}
				else if (Fallback.IsEmpty())
				{
					Fallback = Section.Value;
				}
			}
		}
		FString Content;
		if (Written.IsEmpty())
		{
			Content = Heading.Hint.IsEmpty() ? FString() : FString::Printf(TEXT("<!-- %s -->"), *Heading.Hint);
		}
		else
		{
			Content = bDecisions ? FString::Join(Written, TEXT("\n")) : Written[0];
		}
		Body += FString::Printf(TEXT("\n## %s\n\n%s\n"), *Heading.Title, *Content);
	}
	// Everything else someone wrote (e.g. the old Vision's References, the old Overview's Summary), in order.
	for (const FDocSections& Doc : Read)
	{
		for (const TPair<FString, FString>& Section : Doc.Sections)
		{
			if (!Used.Contains(Section.Key.ToLower()) && !ProtoBoyDocMap::Clean(Section.Value).IsEmpty())
			{
				Used.Add(Section.Key.ToLower());
				Body += FString::Printf(TEXT("\n## %s\n\n%s\n"), *Section.Key, *Section.Value);
			}
		}
	}

	// <Project>.md keeps the root's other properties. It may be one of the docs being merged; the rest go to the trash.
	const FString Path = GetGameDocPath();
	const bool bPathIsMerged = Read.ContainsByPredicate([&Path](const FDocSections& Doc) { return Doc.Path.Equals(Path, ESearchCase::IgnoreCase); });
	if (!bPathIsMerged && Provider.DocExists(Path))
	{
		OutError = FText::Format(LOCTEXT("CombineTaken", "There's already a {0} that isn't on the map. Move or rename it, then combine again."), FText::FromString(Path));
		return false;
	}
	TSet<FString> OldIds;
	for (const FDocSections& Doc : Read)
	{
		OldIds.Add(Doc.Id);
	}
	TSet<FString> UsedIds;
	for (const FProtoBoyDocNode& Node : Nodes)
	{
		if (!OldIds.Contains(Node.Id))
		{
			UsedIds.Add(Node.Id);
		}
	}
	const FString NewId = ProtoBoyDocMap::TakeUniqueId(TEXT("game"), UsedIds);
	const FString Text = ProtoBoyMarkdown::SetFrontmatterValues(Read[0].Frontmatter + Body, {
		{ ProtoBoyDocLayers::IdKey, NewId },
		{ ProtoBoyDocLayers::LayerKey, ProtoBoyDocLayers::ToKey(EProtoBoyDocLayer::Game) },
		{ ProtoBoyDocLayers::ParentKey, FString() },
	});
	if (!Provider.WriteDoc(Path, Text, /*bMustNotExist*/ false, OutError))
	{
		return false;
	}

	// Docs that named an old Game doc as their parent now sit under the new one by default.
	TArray<FString> Children;
	for (const FProtoBoyDocNode& Node : Nodes)
	{
		if (!OldIds.Contains(Node.Id) && OldIds.Contains(Node.ParentId))
		{
			Children.Add(Node.Path);
		}
	}
	bool bOk = true;
	for (const FString& ChildPath : Children)
	{
		FString ChildText;
		bOk &= Provider.ReadDoc(ChildPath, ChildText, OutError)
			&& Provider.WriteDoc(ChildPath, ProtoBoyMarkdown::SetFrontmatterValues(ChildText, { { ProtoBoyDocLayers::ParentKey, FString() } }), false, OutError);
	}
	for (const FDocSections& Doc : Read)
	{
		if (!Doc.Path.Equals(Path, ESearchCase::IgnoreCase))
		{
			bOk &= Provider.DeleteDoc(Doc.Path, OutError);
		}
	}
	AfterWrite();
	return bOk;
}

#undef LOCTEXT_NAMESPACE
