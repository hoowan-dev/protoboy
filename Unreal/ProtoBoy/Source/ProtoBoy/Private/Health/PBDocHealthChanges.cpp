#include "Health/PBDocHealthChanges.h"

#include "Agents/PBAgentPromptContext.h"
#include "Agents/PBAgentRun.h"
#include "Docs/PBDocMap.h"
#include "Docs/PBDocsProvider.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "Misc/Paths.h"
#include "PBSubsystem.h"

#define LOCTEXT_NAMESPACE "ProtoBoyDocHealthChanges"

namespace ProtoBoyDocHealthChanges
{
	/** The project folders whose changes can make a doc stale. */
	const TCHAR* const WatchedFolders = TEXT("Source Content Plugins Config");
	/** Commits read from the history; older ones are ignored. */
	constexpr int32 MaxCommits = 2000;
	/** Changed files kept (the newest). */
	constexpr int32 MaxFiles = 5000;
	/** Shorter file names ("UI", "Hud") match too much text to count as the doc naming them. */
	constexpr int32 MinNameChars = 4;
	/** Shorter paths ("a/b") aren't folder references. */
	constexpr int32 MinPathChars = 6;

	/** Run git in the project folder. False if it couldn't start or failed; OutErr then has its message. */
	bool RunGit(const FString& Args, FString& OutStdOut, FString& OutErr)
	{
		int32 Code = -1;
		// quotepath off: paths with non-ASCII characters come out as they are, not as escaped octal.
		const FString Full = TEXT("-c core.quotepath=false ") + Args;
		OutStdOut.Reset();
		OutErr.Reset();
		return FPlatformProcess::ExecProcess(TEXT("git"), *Full, &Code, &OutStdOut, &OutErr, *FProtoBoyAgentRun::GetProjectDir()) && Code == 0;
	}

	/** The files being collected, by path. */
	struct FFileSet
	{
		TArray<FProtoBoyChangedFile> Files;
		TMap<FString, int32> ByPath;

		FProtoBoyChangedFile& FindOrAdd(const FString& InPath, bool& bOutAdded)
		{
			FString Path = InPath.TrimStartAndEnd();
			Path.ReplaceInline(TEXT("\\"), TEXT("/"));
			if (const int32* Index = ByPath.Find(Path))
			{
				bOutAdded = false;
				return Files[*Index];
			}
			bOutAdded = true;
			ByPath.Add(Path, Files.Num());
			FProtoBoyChangedFile& File = Files.AddDefaulted_GetRef();
			File.Path = Path;
			File.ChangedUtc = FDateTime::MinValue();
			return File;
		}
	};

	/** Commits since SinceUtc, newest first: a header line (\x01<unix time>\x02<subject>), then one "<status>\t<path>" line per file. */
	bool ReadCommits(const FDateTime& SinceUtc, FFileSet& Set, FText& OutError)
	{
		FString Out, Err;
		const FString Args = FString::Printf(TEXT("log --since=%s --max-count=%d --name-status --no-renames --relative --pretty=format:%%x01%%ct%%x02%%s -- %s"),
			*SinceUtc.ToString(TEXT("%Y-%m-%dT%H:%M:%SZ")), MaxCommits, WatchedFolders);
		if (!RunGit(Args, Out, Err))
		{
			OutError = Err.TrimStartAndEnd().IsEmpty()
				? LOCTEXT("NoGit", "Couldn't run git. Make sure it's installed and on your PATH, then restart the editor.")
				: FText::Format(LOCTEXT("GitFailed", "git couldn't read the project's history: {0}"), FText::FromString(Err.TrimStartAndEnd().Left(300)));
			return false;
		}

		TArray<FString> Lines;
		Out.ParseIntoArrayLines(Lines);
		FDateTime CommitUtc = FDateTime::MinValue();
		FString Subject;
		for (const FString& Line : Lines)
		{
			if (Line.StartsWith(TEXT("\x01")))
			{
				FString Time;
				if (!Line.Mid(1).Split(TEXT("\x02"), &Time, &Subject))
				{
					Time = Line.Mid(1);
					Subject.Reset();
				}
				CommitUtc = FDateTime::FromUnixTimestamp(FCString::Atoi64(*Time));
				continue;
			}
			FString Status, Path;
			if (!Line.Split(TEXT("\t"), &Status, &Path) || Path.TrimStartAndEnd().IsEmpty())
			{
				continue;
			}
			bool bAdded = false;
			FProtoBoyChangedFile& File = Set.FindOrAdd(Path, bAdded);
			++File.NumCommits;
			// Newest first: the first commit seen for a file is its latest.
			if (bAdded)
			{
				File.ChangedUtc = CommitUtc;
				File.Subject = Subject.TrimStartAndEnd();
			}
		}
		return true;
	}

	/** A file with local changes: dated by the file itself (now, if it was deleted). */
	void AddUncommitted(const FString& Path, const FDateTime& SinceUtc, FFileSet& Set)
	{
		if (Path.TrimStartAndEnd().IsEmpty())
		{
			return;
		}
		FDateTime ChangedUtc = IFileManager::Get().GetTimeStamp(*(FProtoBoyAgentRun::GetProjectDir() / Path.TrimStartAndEnd()));
		if (ChangedUtc == FDateTime::MinValue())
		{
			ChangedUtc = FDateTime::UtcNow();
		}
		// Untracked files that have sat there since before the oldest doc aren't news.
		if (ChangedUtc <= SinceUtc)
		{
			return;
		}
		bool bAdded = false;
		FProtoBoyChangedFile& File = Set.FindOrAdd(Path, bAdded);
		File.bUncommitted = true;
		File.ChangedUtc = FMath::Max(File.ChangedUtc, ChangedUtc);
	}

	/** Modified and deleted tracked files, then untracked ones. Best effort: a repository without commits has no HEAD to compare with. */
	void ReadUncommitted(const FDateTime& SinceUtc, FFileSet& Set)
	{
		FString Out, Err;
		TArray<FString> Lines;
		if (RunGit(FString::Printf(TEXT("diff --name-status --no-renames --relative HEAD -- %s"), WatchedFolders), Out, Err))
		{
			Out.ParseIntoArrayLines(Lines);
			for (const FString& Line : Lines)
			{
				FString Status, Path;
				if (Line.Split(TEXT("\t"), &Status, &Path))
				{
					AddUncommitted(Path, SinceUtc, Set);
				}
			}
		}
		if (RunGit(FString::Printf(TEXT("ls-files --others --exclude-standard -- %s"), WatchedFolders), Out, Err))
		{
			Lines.Reset();
			Out.ParseIntoArrayLines(Lines);
			for (const FString& Line : Lines)
			{
				AddUncommitted(Line, SinceUtc, Set);
			}
		}
	}

	/** What a doc's text refers to, lower case. */
	struct FDocNames
	{
		/** Identifiers ("herocharacter", "bp_hero"); a class name also counts without its prefix (AHeroCharacter). */
		TSet<FString> Words;
		/** Paths with at least one '/' ("source/game/combat"); /Game/... is read as Content/... */
		TArray<FString> Paths;
	};

	FDocNames ReadNames(const FString& Text)
	{
		FDocNames Names;
		FString Word, Path;
		auto EndWord = [&Names, &Word]()
		{
			if (Word.Len() >= MinNameChars)
			{
				if (Word.Len() > MinNameChars && FCString::Strchr(TEXT("AUFSETI"), Word[0]) && FChar::IsUpper(Word[1]))
				{
					Names.Words.Add(Word.Mid(1).ToLower());
				}
				Names.Words.Add(Word.ToLower());
			}
			Word.Reset();
		};
		auto EndPath = [&Names, &Path]()
		{
			if (Path.Contains(TEXT("/")))
			{
				FString Clean = Path.ToLower();
				while (!Clean.IsEmpty() && (Clean[0] == TEXT('/') || Clean[0] == TEXT('.')))
				{
					Clean.RemoveAt(0);
				}
				while (!Clean.IsEmpty() && (Clean[Clean.Len() - 1] == TEXT('/') || Clean[Clean.Len() - 1] == TEXT('.')))
				{
					Clean.RemoveAt(Clean.Len() - 1);
				}
				if (Clean.StartsWith(TEXT("game/")))
				{
					Clean = TEXT("content/") + Clean.Mid(5);
				}
				if (Clean.Len() >= MinPathChars && Clean.Contains(TEXT("/")))
				{
					Names.Paths.AddUnique(Clean);
				}
			}
			Path.Reset();
		};

		for (int32 Index = 0; Index < Text.Len(); ++Index)
		{
			const TCHAR Char = Text[Index] == TEXT('\\') ? TEXT('/') : Text[Index];
			const bool bWordChar = FChar::IsAlnum(Char) || Char == TEXT('_');
			if (bWordChar)
			{
				Word.AppendChar(Char);
			}
			else
			{
				EndWord();
			}
			if (bWordChar || Char == TEXT('/') || Char == TEXT('.') || Char == TEXT('-'))
			{
				Path.AppendChar(Char);
			}
			else
			{
				EndPath();
			}
		}
		EndWord();
		EndPath();
		return Names;
	}

	/** The doc names the file itself (or its class), or a folder it's in. LowerPath and LowerName are the file's. */
	bool IsNamed(const FDocNames& Names, const FString& LowerPath, const FString& LowerName)
	{
		if (LowerName.Len() >= MinNameChars && Names.Words.Contains(LowerName))
		{
			return true;
		}
		for (const FString& Path : Names.Paths)
		{
			if (LowerPath.Contains(Path, ESearchCase::CaseSensitive))
			{
				return true;
			}
		}
		return false;
	}

	bool IsProviderSupported(FText& OutWhyNot)
	{
		const ISourceControlModule& Module = ISourceControlModule::Get();
		const FString Provider = Module.IsEnabled() ? Module.GetProvider().GetName().ToString() : FString();
		// "Git", and the Git LFS plugins that name themselves "Git LFS 2".
		if (Provider.StartsWith(TEXT("Git")))
		{
			return true;
		}
		OutWhyNot = Provider.IsEmpty()
			? LOCTEXT("NoProvider", "Connect the editor to revision control first.")
			: FText::Format(LOCTEXT("UnsupportedProvider", "Health checks can only read Git history so far, and this project uses {0}."), FText::FromString(Provider));
		return false;
	}

	bool Collect(FProtoBoyDocHealthChanges& Out, FText& OutError)
	{
		Out = FProtoBoyDocHealthChanges();
		const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
		if (!Subsystem)
		{
			OutError = LOCTEXT("NoSubsystem", "ProtoBoy isn't running.");
			return false;
		}
		if (!Subsystem->GetDocsProvider().IsReady(OutError) || !IsProviderSupported(OutError))
		{
			return false;
		}

		// Each doc's last edit is its file's date, or when it was last marked as up to date if that's later.
		TMap<FString, FDateTime> ModifiedByPath;
		for (const FProtoBoyDocInfo& Info : Subsystem->GetDocsProvider().ListDocs())
		{
			ModifiedByPath.Add(Info.Path, Info.ModifiedUtc);
		}
		const TArray<FProtoBoyDocNode>& Nodes = Subsystem->GetDocMap().GetNodes();
		for (const FProtoBoyDocNode& Node : Nodes)
		{
			FDateTime* Modified = ModifiedByPath.Find(Node.Path);
			if (Modified && Node.ReviewedUtc.IsSet())
			{
				*Modified = FMath::Max(*Modified, Node.ReviewedUtc.GetValue());
			}
		}
		FDateTime OldestUtc = FDateTime::MaxValue();
		for (const FProtoBoyDocNode& Node : Nodes)
		{
			if (const FDateTime* Modified = ModifiedByPath.Find(Node.Path))
			{
				OldestUtc = FMath::Min(OldestUtc, *Modified);
				++Out.NumDocsChecked;
			}
		}
		if (Out.NumDocsChecked == 0)
		{
			OutError = LOCTEXT("NoDocs", "There are no docs on the map to check yet.");
			return false;
		}

		// One history query from the oldest doc on; each doc then takes the files newer than itself.
		FFileSet Set;
		if (!ReadCommits(OldestUtc, Set, OutError))
		{
			return false;
		}
		ReadUncommitted(OldestUtc, Set);
		Set.Files.Sort([](const FProtoBoyChangedFile& A, const FProtoBoyChangedFile& B) { return A.ChangedUtc > B.ChangedUtc; });
		if (Set.Files.Num() > MaxFiles)
		{
			Set.Files.SetNum(MaxFiles);
		}
		Out.Files = MoveTemp(Set.Files);
		if (Out.Files.IsEmpty())
		{
			return true;
		}

		TArray<FString> LowerPaths, LowerNames;
		for (const FProtoBoyChangedFile& File : Out.Files)
		{
			LowerPaths.Add(File.Path.ToLower());
			LowerNames.Add(FPaths::GetBaseFilename(File.Path).ToLower());
		}
		for (const FProtoBoyDocNode& Node : Nodes)
		{
			const FDateTime* Modified = ModifiedByPath.Find(Node.Path);
			if (!Modified)
			{
				continue;
			}
			FProtoBoyDocChanges Doc;
			while (Doc.NumChanged < Out.Files.Num() && Out.Files[Doc.NumChanged].ChangedUtc > *Modified)
			{
				++Doc.NumChanged;
			}
			if (Doc.NumChanged == 0)
			{
				continue;
			}
			Doc.DocId = Node.Id;
			Doc.Path = Node.Path;
			Doc.Title = Node.Title;
			Doc.Layer = Node.Layer;
			Doc.ModifiedUtc = *Modified;

			// Without the frontmatter and ProtoBoy's hints, which name things the doc itself doesn't.
			const FDocNames Names = ReadNames(ProtoBoyAgentPrompt::ReadDoc(Node, MAX_int32));
			for (int32 Index = 0; Index < Doc.NumChanged; ++Index)
			{
				if (IsNamed(Names, LowerPaths[Index], LowerNames[Index]))
				{
					Doc.Named.Add(Index);
				}
			}
			Out.Docs.Add(MoveTemp(Doc));
		}
		return true;
	}
}

#undef LOCTEXT_NAMESPACE
