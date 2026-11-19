#include "Health/PBDocStructure.h"

#include "Agents/PBAgentRun.h"
#include "Algo/AnyOf.h"
#include "Docs/PBDocLayers.h"
#include "Docs/PBDocMap.h"
#include "Docs/PBDocsProvider.h"
#include "Docs/PBMarkdown.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "PBSubsystem.h"

#define LOCTEXT_NAMESPACE "ProtoBoyDocStructure"

namespace ProtoBoyDocStructure
{
	/** Details listed per issue; the rest are summed up in one "...and N more" line. */
	constexpr int32 MaxDetails = 20;
	/** The Game doc's generated index: empty only when there are no other docs. */
	const TCHAR* const AreasHeading = TEXT("Areas");
	const TCHAR* const PillarsHeading = TEXT("Design Pillars");
	/** Project folders a doc can name, relative to the project folder. */
	const TCHAR* const ProjectFolders[] = { TEXT("Source/"), TEXT("Content/"), TEXT("Config/"), TEXT("Plugins/") };
	/** Content folders the engine or the editor makes: nobody documents these. */
	const TCHAR* const IgnoredContentFolders[] = { TEXT("Collections"), TEXT("Developers"), TEXT("Localization"), TEXT("Movies"), TEXT("Splash") };

	FProtoBoyDocIssue MakeIssue(FProtoBoyDocIssue::EKind Kind, bool bWarning, const FProtoBoyDocNode* Doc, const FText& Message, TArray<FString> Details = {})
	{
		FProtoBoyDocIssue Issue;
		Issue.Kind = Kind;
		Issue.bWarning = bWarning;
		if (Doc)
		{
			Issue.DocId = Doc->Id;
			Issue.DocTitle = Doc->Title;
			Issue.DocPath = Doc->Path;
		}
		Issue.Message = Message;
		if (Details.Num() > MaxDetails)
		{
			const int32 More = Details.Num() - MaxDetails;
			Details.SetNum(MaxDetails);
			Details.Add(FText::Format(LOCTEXT("MoreDetails", "...and {0} more"), More).ToString());
		}
		Issue.Details = MoveTemp(Details);
		return Issue;
	}

	/** Lower-case names of the notes a wikilink can point at: the whole vault with Obsidian, else the docs folder. */
	TSet<FString> CollectNoteNames(const UProtoBoySubsystem& Subsystem)
	{
		TSet<FString> Names;
		if (Subsystem.GetObsidianConnector().IsConnected())
		{
			TArray<FString> Files;
			IFileManager::Get().FindFilesRecursive(Files, *Subsystem.GetObsidianConnector().GetVaultPath(), TEXT("*.md"), /*Files*/ true, /*Directories*/ false);
			for (FString& File : Files)
			{
				FPaths::NormalizeFilename(File);
				// Deleted notes and Obsidian's own files aren't link targets.
				if (!File.Contains(TEXT("/.trash/")) && !File.Contains(TEXT("/.obsidian/")))
				{
					Names.Add(FPaths::GetBaseFilename(File).ToLower());
				}
			}
		}
		for (const FProtoBoyDocInfo& Info : Subsystem.GetDocsProvider().ListDocs())
		{
			Names.Add(FPaths::GetBaseFilename(Info.Path).ToLower());
		}
		return Names;
	}

	/** The notes Body's [[wikilinks]] point at, as written. Embedded files (![[image.png]]) and same-note links are skipped. */
	TArray<FString> FindLinkTargets(const FString& Body)
	{
		TArray<FString> Targets;
		int32 Pos = 0;
		while (Pos < Body.Len())
		{
			const int32 Open = Body.Find(TEXT("[["), ESearchCase::CaseSensitive, ESearchDir::FromStart, Pos);
			if (Open == INDEX_NONE || Open + 2 >= Body.Len())
			{
				break;
			}
			const int32 Close = Body.Find(TEXT("]]"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Open + 2);
			if (Close == INDEX_NONE)
			{
				break;
			}
			FString Target = Body.Mid(Open + 2, Close - Open - 2);
			if (Target.Contains(TEXT("\n")))
			{
				// Not a link: "[[" with its "]]" on a later line.
				Pos = Open + 2;
				continue;
			}
			Pos = Close + 2;

			// [[Note#Heading|Shown text]], [[Note^block]]: only the note matters.
			int32 Cut;
			for (const TCHAR Separator : { TEXT('|'), TEXT('#'), TEXT('^') })
			{
				if (Target.FindChar(Separator, Cut))
				{
					Target.LeftInline(Cut);
				}
			}
			// In tables the pipe is written "\|".
			Target.RemoveFromEnd(TEXT("\\"));
			Target.TrimStartAndEndInline();
			const FString Extension = FPaths::GetExtension(Target);
			if (!Target.IsEmpty() && (Extension.IsEmpty() || Extension.Equals(TEXT("md"), ESearchCase::IgnoreCase)))
			{
				Targets.AddUnique(Target);
			}
		}
		return Targets;
	}

	/** Body without its [[wikilinks]]: their targets are docs paths ("Content/Ammo"), not project paths. */
	FString RemoveLinks(const FString& Body)
	{
		FString Out;
		int32 Pos = 0;
		while (Pos < Body.Len())
		{
			const int32 Open = Body.Find(TEXT("[["), ESearchCase::CaseSensitive, ESearchDir::FromStart, Pos);
			const int32 Close = Open == INDEX_NONE || Open + 2 >= Body.Len() ? INDEX_NONE : Body.Find(TEXT("]]"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Open + 2);
			if (Close == INDEX_NONE)
			{
				break;
			}
			Out += Body.Mid(Pos, Open - Pos) + TEXT(" ");
			Pos = Close + 2;
		}
		return Pos < Body.Len() ? Out + Body.Mid(Pos) : Out;
	}

	/** The project paths Text names, as written: "Source/Game/Hero.cpp", "Content/Maps", "/Game/Characters/BP_Hero". */
	TArray<FString> FindProjectPaths(const FString& Text)
	{
		TArray<FString> Paths;
		FString Token;
		auto EndToken = [&Paths, &Token]()
		{
			while (!Token.IsEmpty() && (Token.EndsWith(TEXT(".")) || Token.EndsWith(TEXT("/")) || Token.EndsWith(TEXT("-"))))
			{
				Token.LeftChopInline(1);
			}
			bool bProjectPath = Token.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase);
			for (const TCHAR* Folder : ProjectFolders)
			{
				bProjectPath |= Token.StartsWith(Folder, ESearchCase::IgnoreCase);
			}
			// "Source/" on its own names no particular thing.
			TArray<FString> Segments;
			Token.ParseIntoArray(Segments, TEXT("/"));
			if (bProjectPath && Segments.Num() >= 2)
			{
				Paths.AddUnique(Token);
			}
			Token.Reset();
		};
		for (int32 Index = 0; Index < Text.Len(); ++Index)
		{
			const TCHAR Char = Text[Index] == TEXT('\\') ? TEXT('/') : Text[Index];
			if (FChar::IsAlnum(Char) || Char == TEXT('_') || Char == TEXT('/') || Char == TEXT('.') || Char == TEXT('-'))
			{
				Token.AppendChar(Char);
			}
			else
			{
				EndToken();
			}
		}
		EndToken();
		return Paths;
	}

	/** True if a path from FindProjectPaths is there: a file, a folder, or a file of that name with any extension (an asset, a .h/.cpp pair). */
	bool ProjectPathExists(const FString& Path)
	{
		FString Relative = Path;
		if (Relative.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase))
		{
			// /Game/Characters/BP_Hero.BP_Hero is the asset Content/Characters/BP_Hero.uasset.
			Relative = TEXT("Content/") + Relative.Mid(6);
			int32 Slash = INDEX_NONE;
			int32 Dot = INDEX_NONE;
			Relative.FindLastChar(TEXT('/'), Slash);
			if (Relative.FindLastChar(TEXT('.'), Dot) && Dot > Slash)
			{
				Relative.LeftInline(Dot);
			}
		}
		const FString Absolute = FProtoBoyAgentRun::GetProjectDir() / Relative;
		if (FPaths::FileExists(Absolute) || FPaths::DirectoryExists(Absolute))
		{
			return true;
		}
		if (!FPaths::GetExtension(Relative).IsEmpty())
		{
			return false;
		}
		TArray<FString> Found;
		IFileManager::Get().FindFiles(Found, *(Absolute + TEXT(".*")), /*Files*/ true, /*Directories*/ false);
		return !Found.IsEmpty();
	}

	/** Add Text's words (letters, digits, '_'), lower case. */
	void AddWords(const FString& Text, TSet<FString>& Words)
	{
		FString Word;
		for (int32 Index = 0; Index <= Text.Len(); ++Index)
		{
			const TCHAR Char = Index < Text.Len() ? Text[Index] : TEXT(' ');
			if (FChar::IsAlnum(Char) || Char == TEXT('_'))
			{
				Word.AppendChar(FChar::ToLower(Char));
			}
			else if (!Word.IsEmpty())
			{
				Words.Add(Word);
				Word.Reset();
			}
		}
	}

	/** The "## " sections of a doc with nothing written under them. OutTotal counts the sections looked at. */
	TArray<FString> FindUnwrittenSections(const FString& Body, bool bGameDoc, int32& OutTotal)
	{
		TArray<FString> Unwritten;
		OutTotal = 0;
		for (const ProtoBoyMarkdown::FSection& Section : ProtoBoyMarkdown::ParseSections(Body))
		{
			// An empty Decisions section is normal, and Areas is ProtoBoy's to fill.
			if (Section.Level != 2 || Section.Heading.Equals(ProtoBoyDocLayers::GetDecisionsHeadingTitle(), ESearchCase::IgnoreCase)
				|| (bGameDoc && Section.Heading.Equals(AreasHeading, ESearchCase::IgnoreCase)))
			{
				continue;
			}
			++OutTotal;
			if (Body.Mid(Section.BodyStart, Section.End - Section.BodyStart).TrimStartAndEnd().IsEmpty())
			{
				Unwritten.Add(Section.Heading);
			}
		}
		return Unwritten;
	}

	/** Subfolders of a project folder, by name. */
	TArray<FString> ListFolders(const FString& ProjectRelativeFolder)
	{
		TArray<FString> Folders;
		IFileManager::Get().FindFiles(Folders, *(FProtoBoyAgentRun::GetProjectDir() / ProjectRelativeFolder / TEXT("*")), /*Files*/ false, /*Directories*/ true);
		Folders.Sort();
		return Folders;
	}

	TArray<FProtoBoyDocIssue> Check()
	{
		TArray<FProtoBoyDocIssue> Issues;
		const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
		FText WhyNot;
		if (!Subsystem || !Subsystem->GetDocsProvider().IsReady(WhyNot))
		{
			return Issues;
		}
		const IProtoBoyDocsProvider& Provider = Subsystem->GetDocsProvider();
		const FProtoBoyDocMap& Map = Subsystem->GetDocMap();
		const TArray<FProtoBoyDocNode>& Nodes = Map.GetNodes();
		const FProtoBoyDocNode* Game = Map.GetGame();

		if (!Game)
		{
			Issues.Add(MakeIssue(FProtoBoyDocIssue::EKind::NoGameDoc, true, nullptr,
				LOCTEXT("NoGameDoc", "There's no Game doc, so the doc map has no root. Create it from the Documentation page.")));
		}
		if (Map.HasSplitGameDocs())
		{
			TArray<FString> Paths;
			for (const FProtoBoyDocNode* Doc : Map.GetGameDocs())
			{
				Paths.Add(Doc->Path);
			}
			Issues.Add(MakeIssue(FProtoBoyDocIssue::EKind::SplitGameDocs, true, nullptr,
				LOCTEXT("SplitGameDocs", "There are several Game docs (from an older version). Combine them from the Documentation page."), MoveTemp(Paths)));
		}

		// Lower-case doc paths, to tell a docs path ("Content/Ammo.md") from a project path ("Content/Maps").
		TArray<FString> DocPaths;
		for (const FProtoBoyDocInfo& Info : Provider.ListDocs())
		{
			DocPaths.Add(Info.Path.ToLower());
		}
		const TSet<FString> NoteNames = CollectNoteNames(*Subsystem);
		TSet<FString> Words;

		for (const FProtoBoyDocNode& Node : Nodes)
		{
			if (!Node.ParentId.IsEmpty() && !Map.Find(Node.ParentId))
			{
				Issues.Add(MakeIssue(FProtoBoyDocIssue::EKind::MissingParent, true, &Node,
					FText::Format(LOCTEXT("MissingParent", "Its parent (\"{0}\") isn't on the map, so it sits under the Game doc. Move it on the doc map to fix its frontmatter."), FText::FromString(Node.ParentId))));
			}

			FString Text;
			FText Ignored;
			if (!Provider.ReadDoc(Node.Path, Text, Ignored))
			{
				continue;
			}
			// What a reader wrote: no frontmatter, no hints.
			const FString Body = ProtoBoyMarkdown::StripComments(Text.Mid(ProtoBoyMarkdown::ParseFrontmatter(Text).BodyStart));
			AddWords(Body, Words);

			TArray<FString> Broken;
			for (const FString& Target : FindLinkTargets(Body))
			{
				if (!NoteNames.Contains(FPaths::GetBaseFilename(Target).ToLower()))
				{
					Broken.Add(Target);
				}
			}
			if (!Broken.IsEmpty())
			{
				Issues.Add(MakeIssue(FProtoBoyDocIssue::EKind::BrokenLinks, true, &Node,
					FText::Format(LOCTEXT("BrokenLinks", "{0} {0}|plural(one=link points,other=links point) at {0}|plural(one=a note that doesn't,other=notes that don't) exist."), Broken.Num()), MoveTemp(Broken)));
			}

			TArray<FString> Missing;
			for (const FString& Path : FindProjectPaths(RemoveLinks(Body)))
			{
				const FString Lower = Path.ToLower();
				const bool bDocsPath = Lower.EndsWith(TEXT(".md")) || DocPaths.ContainsByPredicate([&Lower](const FString& DocPath) { return DocPath.StartsWith(Lower); });
				if (!bDocsPath && !ProjectPathExists(Path))
				{
					Missing.Add(Path);
				}
			}
			if (!Missing.IsEmpty())
			{
				Issues.Add(MakeIssue(FProtoBoyDocIssue::EKind::MissingPaths, true, &Node,
					FText::Format(LOCTEXT("MissingPaths", "It names {0} project {0}|plural(one=path that doesn't,other=paths that don't) exist (renamed, moved, removed, or not made yet)."), Missing.Num()), MoveTemp(Missing)));
			}

			const bool bGameDoc = &Node == Game;
			int32 NumSections = 0;
			TArray<FString> Unwritten = FindUnwrittenSections(Body, bGameDoc, NumSections);
			// The example pillar a new Game doc starts with isn't one of the game's.
			if (bGameDoc && Map.ReadGame().Pillars.IsEmpty())
			{
				Unwritten.AddUnique(PillarsHeading);
			}
			if (NumSections > 0 && Unwritten.Num() >= NumSections)
			{
				Issues.Add(MakeIssue(FProtoBoyDocIssue::EKind::Unwritten, false, &Node, LOCTEXT("NothingWritten", "Nothing is written in it yet.")));
			}
			else if (!Unwritten.IsEmpty())
			{
				Issues.Add(MakeIssue(FProtoBoyDocIssue::EKind::Unwritten, false, &Node,
					FText::Format(LOCTEXT("Unwritten", "{0} {0}|plural(one=section isn't,other=sections aren't) written yet."), Unwritten.Num()), MoveTemp(Unwritten)));
			}
		}

		// Design docs at the bottom of the design tree with nothing saying how they're built or what assets they need.
		TFunction<bool(const FString&, int32)> HasTechOrContent = [&Map, &HasTechOrContent](const FString& Id, int32 Depth)
		{
			for (const FProtoBoyDocNode* Child : Map.GetChildren(Id))
			{
				// The map has no cycles, but don't trust that blindly.
				if (Child->Layer == EProtoBoyDocLayer::Tech || Child->Layer == EProtoBoyDocLayer::Content || (Depth < 32 && HasTechOrContent(Child->Id, Depth + 1)))
				{
					return true;
				}
			}
			return false;
		};
		for (const FProtoBoyDocNode& Node : Nodes)
		{
			const bool bHasDesignBelow = Map.GetChildren(Node.Id).ContainsByPredicate([](const FProtoBoyDocNode* Child) { return Child->Layer == EProtoBoyDocLayer::Design; });
			if (Node.Layer == EProtoBoyDocLayer::Design && !bHasDesignBelow && !HasTechOrContent(Node.Id, 0))
			{
				Issues.Add(MakeIssue(FProtoBoyDocIssue::EKind::NoTechOrContent, false, &Node,
					LOCTEXT("NoTechOrContent", "No Tech or Content doc sits below this Design doc: nothing says how it's built or what assets it needs.")));
			}
		}

		if (!Map.GetUnmappedDocs().IsEmpty())
		{
			TArray<FString> Paths;
			for (const FProtoBoyDocInfo& Info : Map.GetUnmappedDocs())
			{
				Paths.Add(Info.Path);
			}
			Issues.Add(MakeIssue(FProtoBoyDocIssue::EKind::UnmappedNotes, false, nullptr,
				FText::Format(LOCTEXT("UnmappedNotes", "{0} {0}|plural(one=note,other=notes) in the docs folder {0}|plural(one=isn't,other=aren't) on the doc map. Link them from the Documentation page, or leave them if they're not project docs."), Paths.Num()), MoveTemp(Paths)));
		}

		// Parts of the project no doc mentions by name. Only once there are docs to mention them.
		if (!Nodes.IsEmpty())
		{
			TArray<FString> Undocumented;
			for (const FString& Module : ListFolders(TEXT("Source")))
			{
				if (!Words.Contains(Module.ToLower()))
				{
					Undocumented.Add(TEXT("Source/") + Module);
				}
			}
			for (const FString& Folder : ListFolders(TEXT("Content")))
			{
				const bool bIgnored = Folder.StartsWith(TEXT("__")) || Algo::AnyOf(IgnoredContentFolders, [&Folder](const TCHAR* Ignored) { return Folder.Equals(Ignored, ESearchCase::IgnoreCase); });
				if (!bIgnored && !Words.Contains(Folder.ToLower()))
				{
					Undocumented.Add(TEXT("Content/") + Folder);
				}
			}
			if (!Undocumented.IsEmpty())
			{
				Issues.Add(MakeIssue(FProtoBoyDocIssue::EKind::UndocumentedFolders, false, nullptr,
					FText::Format(LOCTEXT("UndocumentedFolders", "No doc mentions {0} project {0}|plural(one=folder,other=folders) by name."), Undocumented.Num()), MoveTemp(Undocumented)));
			}
		}

		Issues.StableSort([](const FProtoBoyDocIssue& A, const FProtoBoyDocIssue& B) { return A.bWarning && !B.bWarning; });
		return Issues;
	}
}

#undef LOCTEXT_NAMESPACE
