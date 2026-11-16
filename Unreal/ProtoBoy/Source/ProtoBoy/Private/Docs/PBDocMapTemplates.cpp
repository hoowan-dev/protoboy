// Replacing the documentation with a doc-set template (or clearing it for generated docs). Part of FProtoBoyDocMap.
#include "Docs/PBDocMap.h"

#include "Docs/PBDocsService.h"
#include "Docs/PBDocMapUtils.h"
#include "Docs/PBDocTemplates.h"
#include "Docs/PBMarkdown.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "PBModule.h"

#define LOCTEXT_NAMESPACE "ProtoBoyDocMapTemplates"

bool FProtoBoyDocMap::ClearDocs(FText& OutError)
{
	IProtoBoyDocsProvider& Provider = Docs.GetProvider();
	if (!Provider.IsReady(OutError))
	{
		return false;
	}
	Load();
	TArray<FString> Paths;
	for (const FProtoBoyDocNode& Node : Nodes)
	{
		Paths.Add(Node.Path);
	}
	bool bOk = true;
	for (const FString& Path : Paths)
	{
		bOk &= Provider.DeleteDoc(Path, OutError);
	}
	for (const FString& Folder : ProtoBoyDocMap::GetReplaceableFolders(Paths))
	{
		Provider.PruneEmptyFolders(Folder);
	}
	AfterWrite();
	return bOk;
}

FProtoBoyDocTemplatePlan FProtoBoyDocMap::PlanTemplate(const FProtoBoyDocTemplate& Template) const
{
	// Read the docs again, so notes added, moved or renamed in Obsidian since the last refresh are replaced too.
	Load();
	FProtoBoyDocTemplatePlan Plan;
	Plan.NumToCreate = 1;
	for (const FProtoBoyDocNode& Node : Nodes)
	{
		Plan.Removed.Add(Node.Path);
	}

	// Files that aren't on the map but sit where the template writes go too, so the template can be written whole.
	const IProtoBoyDocsProvider& Provider = Docs.GetProvider();
	auto RemoveIfTaken = [&Plan, &Provider](const FString& Path)
	{
		if (Provider.DocExists(Path) && !Plan.Removed.ContainsByPredicate([&Path](const FString& Other) { return Other.Equals(Path, ESearchCase::IgnoreCase); }))
		{
			Plan.Removed.Add(Path);
		}
	};
	RemoveIfTaken(GetGameDocPath());
	TMap<FString, FString> Paths;
	TMap<FString, int32> Depths;
	for (const FProtoBoyDocTemplateDoc& Doc : Template.Docs)
	{
		FProtoBoyDocTemplatePlan::FEntry Entry;
		ProtoBoyDocLayers::FromTemplateKey(Doc.Kind, Entry.Kind);
		Entry.Title = Doc.Title.TrimStartAndEnd();
		Entry.Path = ProtoBoyDocMap::MakeNewDocPath(Entry.Kind, Entry.Title, Paths.Find(Doc.Parent));
		Entry.Depth = Doc.Parent.IsEmpty() ? 0 : Depths.FindRef(Doc.Parent) + 1;
		RemoveIfTaken(Entry.Path);
		++Plan.NumToCreate;

		Paths.Add(Doc.Id, Entry.Path);
		Depths.Add(Doc.Id, Entry.Depth);
		Plan.Docs.Add(MoveTemp(Entry));
	}
	return Plan;
}

bool FProtoBoyDocMap::ApplyTemplate(const FProtoBoyDocTemplate& Template, bool bExamples, int32& OutCreated, FText& OutError)
{
	OutCreated = 0;
	IProtoBoyDocsProvider& Provider = Docs.GetProvider();
	if (!Provider.IsReady(OutError))
	{
		return false;
	}
	// Checked before anything is moved to the trash.
	for (const FProtoBoyDocTemplateDoc& Doc : Template.Docs)
	{
		if (!IsAllowedTitle(Doc.Title, OutError))
		{
			return false;
		}
	}

	// Clear first: everything on the map, and whatever sits where the template writes, goes to the trash.
	const FProtoBoyDocTemplatePlan Plan = PlanTemplate(Template);
	for (const FString& Path : Plan.Removed)
	{
		if (!Provider.DeleteDoc(Path, OutError))
		{
			AfterWrite();
			return false;
		}
	}
	Load();

	bool bWrote = false;
	if (!WriteMissingGameDoc(&Template.Pillars, Template.Name, bWrote, OutError, bExamples ? &Template.GameExamples : nullptr, Template.bGenerated))
	{
		AfterWrite();
		return false;
	}
	OutCreated = bWrote ? 1 : 0;
	Load();

	// Written in order, so every parent exists (on disk, and in MapIds) before its children. The map is read again
	// once at the end instead of after every doc.
	const FProtoBoyDocNode* Root = GetGame();
	const FString RootId = Root ? Root->Id : FString();

	TSet<FString> UsedIds;
	for (const FProtoBoyDocNode& Node : Nodes)
	{
		UsedIds.Add(Node.Id);
	}
	TMap<FString, FString> MapIds;  // template doc id -> its id on the map ("" = not on the map)
	TMap<FString, FString> Paths;
	bool bOk = true;
	for (const FProtoBoyDocTemplateDoc& Doc : Template.Docs)
	{
		EProtoBoyDocKind Kind = EProtoBoyDocKind::Design;
		ProtoBoyDocLayers::FromTemplateKey(Doc.Kind, Kind);
		const FString Title = Doc.Title.TrimStartAndEnd();
		const FString Path = ProtoBoyDocMap::MakeNewDocPath(Kind, Title, Paths.Find(Doc.Parent));
		Paths.Add(Doc.Id, Path);

		const FString Id = ProtoBoyDocMap::TakeUniqueId(Title, UsedIds);
		const FString ParentId = Doc.Parent.IsEmpty() ? FString() : MapIds.FindRef(Doc.Parent);
		const FString Text = ProtoBoyMarkdown::SetFrontmatterValues(
			ProtoBoyDocLayers::MakeBody(Title, ProtoBoyDocLayers::GetIntro(Kind), ProtoBoyDocLayers::GetHeadings(Kind),
				bExamples && !Doc.Examples.IsEmpty() ? Doc.Summary + TEXT("\n\n") + ProtoBoyDocMap::MakeExampleNote(Template.Name, Template.bGenerated) : Doc.Summary,
				ProtoBoyDocMap::ToSectionText(bExamples ? &Doc.Examples : nullptr)), {
				{ ProtoBoyDocLayers::IdKey, Id },
				{ ProtoBoyDocLayers::LayerKey, ProtoBoyDocLayers::ToKey(ProtoBoyDocLayers::GetLayer(Kind)) },
				{ ProtoBoyDocLayers::ParentKey, ParentId != RootId ? ParentId : FString() },
			});
		if (!Provider.WriteDoc(Path, Text, /*bMustNotExist*/ true, OutError))
		{
			bOk = false;
			break;
		}
		MapIds.Add(Doc.Id, Id);
		++OutCreated;
		bWrote = true;
	}

	// Folders the old docs leave empty go too, so the old structure doesn't linger. Each delete already removes the
	// folders it empties; this also catches folders emptied earlier (or kept by Obsidian holding them open), but only
	// under the top-level folders the removed docs were in and the kind folders, so other empty folders stay.
	for (const FString& Folder : ProtoBoyDocMap::GetReplaceableFolders(Plan.Removed))
	{
		Provider.PruneEmptyFolders(Folder);
	}

	AfterWrite();
	return bOk;
}

#undef LOCTEXT_NAMESPACE
