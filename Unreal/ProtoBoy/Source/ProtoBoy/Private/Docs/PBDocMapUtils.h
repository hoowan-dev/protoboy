#pragma once

#include "CoreMinimal.h"
#include "Docs/PBDocLayers.h"
#include "Docs/PBDocTemplates.h"
#include "Docs/PBMarkdown.h"
#include "Misc/Paths.h"

/** Helpers shared by FProtoBoyDocMap's files (PBDocMap, PBDocMapTemplates, PBDocMapMigration). */
namespace ProtoBoyDocMap
{
	/** "Design/Combat Basics.md" -> "design-combat-basics". */
	inline FString Slugify(const FString& Text)
	{
		FString Slug;
		bool bDash = false;
		for (const TCHAR C : Text)
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
		return Slug.IsEmpty() ? FString(TEXT("doc")) : Slug.Left(60);
	}

	/** A title made safe to use as a file name (Obsidian also dislikes # ^ [ ] in note names). */
	inline FString ToFileName(const FString& Title)
	{
		FString Out;
		for (const TCHAR C : Title.TrimStartAndEnd())
		{
			if (!FString(TEXT("<>:\"/\\|?*#^[]")).Contains(FString::Chr(C)) && C >= 32)
			{
				Out.AppendChar(C);
			}
		}
		Out.TrimStartAndEndInline();
		while (Out.EndsWith(TEXT(".")))
		{
			Out.LeftChopInline(1);
		}
		return Out.IsEmpty() ? FString(TEXT("Untitled")) : Out.Left(80);
	}

	/** Text with ProtoBoy's managed blocks (generated lists) taken out, markers included. */
	inline FString RemoveManagedBlocks(const FString& Text)
	{
		FString Out = Text;
		int32 Begin;
		while ((Begin = Out.Find(TEXT("<!-- protoboy:begin "), ESearchCase::CaseSensitive)) != INDEX_NONE)
		{
			const int32 EndMarker = Out.Find(TEXT("<!-- protoboy:end "), ESearchCase::CaseSensitive, ESearchDir::FromStart, Begin);
			const int32 EndClose = EndMarker == INDEX_NONE ? INDEX_NONE : Out.Find(TEXT("-->"), ESearchCase::CaseSensitive, ESearchDir::FromStart, EndMarker);
			if (EndClose == INDEX_NONE)
			{
				break;
			}
			Out = Out.Left(Begin) + Out.Mid(EndClose + 3);
		}
		return Out;
	}

	/** What a reader actually wrote in a section: no hints, no generated blocks. */
	inline FString Clean(const FString& Text)
	{
		return ProtoBoyMarkdown::StripComments(RemoveManagedBlocks(Text));
	}

	/** First line below the title that someone wrote (not a heading, quote, hint or generated list). */
	inline FString FindSummary(const FString& Text, int32 BodyStart)
	{
		TArray<FString> Lines;
		Clean(Text.Mid(BodyStart)).ParseIntoArrayLines(Lines);
		for (const FString& Raw : Lines)
		{
			const FString Line = Raw.TrimStartAndEnd();
			if (Line.IsEmpty() || Line.StartsWith(TEXT("#")) || Line.StartsWith(TEXT(">")) || Line.StartsWith(TEXT("---")))
			{
				continue;
			}
			return Line.Len() > 160 ? Line.Left(157) + TEXT("...") : Line;
		}
		return FString();
	}

	/** Sort key for children: layers in their top-to-bottom order. */
	inline int32 LayerOrder(EProtoBoyDocLayer Layer)
	{
		return ProtoBoyDocLayers::GetAllLayers().IndexOfByKey(Layer);
	}

	/** ADRs from the eight-layer version: kept as files, off the map (decisions now live in each doc). */
	inline bool IsRetiredDoc(const FString& Path, const ProtoBoyMarkdown::FFrontmatter& Frontmatter)
	{
		return ProtoBoyDocLayers::IsRetiredKey(Frontmatter.Get(ProtoBoyDocLayers::LayerKey))
			|| (Frontmatter.Get(ProtoBoyDocLayers::LayerKey).IsEmpty() && Path.StartsWith(TEXT("ADR/ADR-"), ESearchCase::IgnoreCase));
	}

	/**
	 * The folder a doc's children go in. A doc that already has its own folder (Design/Combat/Combat.md, or its Tech doc
	 * Combat.tech.md next to it) shares it; a flat doc (Content/Ammo.md) gets a folder named after it (Content/Ammo/).
	 */
	inline FString GetChildFolder(const FString& ParentPath)
	{
		const FString Dir = FPaths::GetPath(ParentPath);
		FString Base = FPaths::GetBaseFilename(ParentPath);
		Base.RemoveFromEnd(TEXT(".tech"), ESearchCase::IgnoreCase);
		return FPaths::GetCleanFilename(Dir).Equals(Base, ESearchCase::IgnoreCase) ? Dir : Dir / Base;
	}

	/**
	 * Where a new doc goes. Under a parent (ParentPath, null for the Game doc) it goes in the parent's child folder;
	 * otherwise in its kind's folder (Design/, Tech/, Content/). Design docs get a folder of their own on top, like the
	 * features they describe (Design/Combat/Weapons/Weapons.md), so their children can nest in it. The Game doc is
	 * at the docs root.
	 */
	inline FString MakeNewDocPath(EProtoBoyDocKind Kind, const FString& Title, const FString* ParentPath)
	{
		if (Kind == EProtoBoyDocKind::Game)
		{
			return FProtoBoyDocMap::GetGameDocPath();
		}
		const FString Name = ToFileName(Title);
		FString Folder = ParentPath ? GetChildFolder(*ParentPath) : ProtoBoyDocLayers::GetFolder(Kind);
		if (Kind == EProtoBoyDocKind::Design)
		{
			Folder /= Name;
		}
		return Folder / Name + TEXT(".md");
	}

	/** Slugify(Title), suffixed until it isn't in Used. Adds the result to Used. */
	inline FString TakeUniqueId(const FString& Title, TSet<FString>& Used)
	{
		const FString Base = Slugify(Title);
		FString Id = Base;
		for (int32 Suffix = 2; Used.Contains(Id); ++Suffix)
		{
			Id = FString::Printf(TEXT("%s-%d"), *Base, Suffix);
		}
		Used.Add(Id);
		return Id;
	}

	/** Template example sections as MakeBody's SectionText. */
	inline TArray<TPair<FString, FString>> ToSectionText(const TArray<FProtoBoyDocTemplateSection>* Examples)
	{
		TArray<TPair<FString, FString>> Out;
		for (const FProtoBoyDocTemplateSection& Section : Examples ? *Examples : TArray<FProtoBoyDocTemplateSection>())
		{
			Out.Emplace(Section.Heading, Section.Text);
		}
		return Out;
	}

	/** The line under a doc's summary that says its sections hold examples (or were drafted by an agent). */
	inline FString MakeExampleNote(const FString& TemplateName, bool bGenerated = false)
	{
		return bGenerated
			? FString::Printf(TEXT("_Drafted by an agent from the project's contents on %s. It's a starting point: check it, and add the details of your game._"), *FDateTime::Now().ToString(TEXT("%Y-%m-%d")))
			: FString::Printf(TEXT("_The sections below hold example content from the %s template. Rewrite them for your game, or delete what doesn't apply._"), *TemplateName);
	}

	/** The kind folders templates write to, plus older versions' ones: emptied folders in them are removed on replace. */
	inline TSet<FString> GetReplaceableFolders(const TArray<FString>& RemovedPaths)
	{
		TSet<FString> Folders = { TEXT("Design"), TEXT("Tech"), TEXT("Content"), TEXT("Systems"), TEXT("Direction") };
		for (const FString& Removed : RemovedPaths)
		{
			FString Top;
			if (Removed.Split(TEXT("/"), &Top, nullptr))
			{
				Folders.Add(Top);
			}
		}
		return Folders;
	}

	inline const TCHAR* const ExamplePillar = TEXT(
		"### Example: Every Shot Counts\n"
		"\n"
		"- **Feeling:** tension; each decision matters.\n"
		"- **Rules out:** ammo pickups everywhere, spray-and-pray weapons.\n");

	/** "Design Pillars" is the third Game heading. */
	inline constexpr int32 PillarsHeadingIndex = 2;
}
