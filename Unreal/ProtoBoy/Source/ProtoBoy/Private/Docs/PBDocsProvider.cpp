#include "Docs/PBDocsProvider.h"

#include "Docs/PBMarkdown.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PBSettings.h"

#define LOCTEXT_NAMESPACE "ProtoBoyDocsProvider"

namespace ProtoBoyDocsProvider
{
	/**
	 * Run a file operation, retrying briefly if it fails. Obsidian opens files and folders right after they change (to
	 * index them), and on Windows a write, move or folder delete fails while another process has them open.
	 */
	bool WithRetries(TFunctionRef<bool()> Operation)
	{
		constexpr int32 Attempts = 8;
		for (int32 Attempt = 0; Attempt < Attempts; ++Attempt)
		{
			if (Operation())
			{
				return true;
			}
			if (Attempt + 1 < Attempts)
			{
				FPlatformProcess::Sleep(0.025f * (Attempt + 1));
			}
		}
		return false;
	}

	/** Delete Dir if it's empty, retrying while Obsidian has it open. */
	bool DeleteIfEmpty(const FString& Dir)
	{
		return WithRetries([&Dir]()
		{
			bool bEmpty = true;
			IFileManager::Get().IterateDirectory(*Dir, [&bEmpty](const TCHAR*, bool) { bEmpty = false; return false; });
			return bEmpty && IFileManager::Get().DeleteDirectory(*Dir);
		});
	}

	/** Remove the empty folders under Dir (bottom-up), then Dir itself if that leaves it empty. True if Dir is gone. */
	bool PruneEmpty(const FString& Dir)
	{
		bool bHasFiles = false;
		TArray<FString> Folders;
		IFileManager::Get().IterateDirectory(*Dir, [&bHasFiles, &Folders](const TCHAR* Path, bool bIsDirectory)
		{
			if (bIsDirectory && !FPaths::GetCleanFilename(Path).StartsWith(TEXT(".")))
			{
				Folders.Add(Path);
			}
			else
			{
				bHasFiles = true;
			}
			return true;
		});
		bool bAllGone = true;
		for (const FString& Folder : Folders)
		{
			bAllGone &= PruneEmpty(Folder);
		}
		return !bHasFiles && bAllGone && DeleteIfEmpty(Dir);
	}
}

bool FProtoBoyFolderDocsProvider::IsReady(FText& OutWhyNot) const
{
	const FString Root = GetDocsRoot();
	if (Root.IsEmpty())
	{
		OutWhyNot = LOCTEXT("NoRoot", "No docs folder is configured.");
		return false;
	}
	return true;
}

bool FProtoBoyFolderDocsProvider::ResolvePath(const FString& RelativePath, FString& OutAbsolutePath, FText& OutError) const
{
	FString Rel = RelativePath.TrimStartAndEnd();
	Rel.ReplaceInline(TEXT("\\"), TEXT("/"));
	while (Rel.StartsWith(TEXT("/")))
	{
		Rel.RightChopInline(1);
	}

	if (Rel.IsEmpty() || !Rel.EndsWith(TEXT(".md"), ESearchCase::IgnoreCase))
	{
		OutError = LOCTEXT("NotMarkdown", "Doc paths must be relative .md files, e.g. \"Design/Combat.md\".");
		return false;
	}
	if (Rel.Contains(TEXT(":")) || Rel.Contains(TEXT("..")) || Rel.Contains(TEXT("//")))
	{
		OutError = LOCTEXT("BadPath", "Doc paths must stay inside the docs folder (no drive letters or \"..\").");
		return false;
	}

	const FString Root = GetDocsRoot();
	FString Absolute = FPaths::ConvertRelativePathToFull(Root / Rel);
	FPaths::NormalizeFilename(Absolute);
	FString NormalizedRoot = FPaths::ConvertRelativePathToFull(Root);
	FPaths::NormalizeDirectoryName(NormalizedRoot);

	if (!Absolute.StartsWith(NormalizedRoot + TEXT("/"), ESearchCase::IgnoreCase))
	{
		OutError = LOCTEXT("Escapes", "Doc paths must stay inside the docs folder.");
		return false;
	}

	OutAbsolutePath = Absolute;
	return true;
}

TArray<FProtoBoyDocInfo> FProtoBoyFolderDocsProvider::ListDocs() const
{
	TArray<FProtoBoyDocInfo> Docs;
	FString Root = FPaths::ConvertRelativePathToFull(GetDocsRoot());
	FPaths::NormalizeDirectoryName(Root);
	if (Root.IsEmpty() || !FPaths::DirectoryExists(Root))
	{
		return Docs;
	}

	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *Root, TEXT("*.md"), true, false);
	for (FString& File : Files)
	{
		FPaths::NormalizeFilename(File);
		FString Rel = File.Mid(Root.Len() + 1);

		// Skip hidden folders/files (".trash", ".obsidian") wherever they appear.
		if (Rel.StartsWith(TEXT(".")) || Rel.Contains(TEXT("/.")))
		{
			continue;
		}

		const FFileStatData Stat = IFileManager::Get().GetStatData(*File);

		FProtoBoyDocInfo Info;
		Info.Path = Rel;
		Info.ModifiedUtc = Stat.ModificationTime;
		Info.SizeBytes = Stat.FileSize;

		FString Text;
		if (Stat.FileSize <= MaxDocBytes && FFileHelper::LoadFileToString(Text, *File))
		{
			Info.Title = ProtoBoyMarkdown::GetTitle(Text);
		}
		if (Info.Title.IsEmpty())
		{
			Info.Title = FPaths::GetBaseFilename(File);
		}
		Docs.Add(MoveTemp(Info));
	}

	Docs.Sort([](const FProtoBoyDocInfo& A, const FProtoBoyDocInfo& B) { return A.Path < B.Path; });
	return Docs;
}

bool FProtoBoyFolderDocsProvider::DocExists(const FString& RelativePath) const
{
	FString Absolute;
	FText Ignored;
	return ResolvePath(RelativePath, Absolute, Ignored) && FPaths::FileExists(Absolute);
}

bool FProtoBoyFolderDocsProvider::ReadDoc(const FString& RelativePath, FString& OutText, FText& OutError) const
{
	FString Absolute;
	if (!ResolvePath(RelativePath, Absolute, OutError))
	{
		return false;
	}
	if (!FPaths::FileExists(Absolute))
	{
		OutError = FText::Format(LOCTEXT("Missing", "No doc at \"{0}\"."), FText::FromString(RelativePath));
		return false;
	}
	if (IFileManager::Get().FileSize(*Absolute) > MaxDocBytes)
	{
		OutError = FText::Format(LOCTEXT("TooBig", "\"{0}\" is larger than 1 MB."), FText::FromString(RelativePath));
		return false;
	}
	if (!FFileHelper::LoadFileToString(OutText, *Absolute))
	{
		OutError = FText::Format(LOCTEXT("ReadFailed", "Couldn't read \"{0}\"."), FText::FromString(RelativePath));
		return false;
	}
	return true;
}

bool FProtoBoyFolderDocsProvider::WriteDoc(const FString& RelativePath, const FString& Text, bool bMustNotExist, FText& OutError)
{
	FString Absolute;
	if (!ResolvePath(RelativePath, Absolute, OutError))
	{
		return false;
	}
	if (bMustNotExist && FPaths::FileExists(Absolute))
	{
		OutError = FText::Format(LOCTEXT("Exists", "\"{0}\" already exists."), FText::FromString(RelativePath));
		return false;
	}
	if (Text.Len() > MaxDocBytes)
	{
		OutError = LOCTEXT("WriteTooBig", "Docs are limited to 1 MB.");
		return false;
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Absolute), true);
	if (!ProtoBoyDocsProvider::WithRetries([&Text, &Absolute]() { return FFileHelper::SaveStringToFile(Text, *Absolute, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM); }))
	{
		OutError = FText::Format(LOCTEXT("WriteFailed", "Couldn't write \"{0}\"."), FText::FromString(RelativePath));
		return false;
	}
	return true;
}

bool FProtoBoyFolderDocsProvider::DeleteDoc(const FString& RelativePath, FText& OutError)
{
	FString Absolute;
	if (!ResolvePath(RelativePath, Absolute, OutError))
	{
		return false;
	}
	if (!FPaths::FileExists(Absolute))
	{
		OutError = FText::Format(LOCTEXT("DeleteMissing", "No doc at \"{0}\"."), FText::FromString(RelativePath));
		return false;
	}

	const FString Trash = GetTrashDir();
	const FString Base = FPaths::GetBaseFilename(Absolute);
	FString Target = FPaths::Combine(Trash, FPaths::GetCleanFilename(Absolute));
	for (int32 Suffix = 2; FPaths::FileExists(Target); ++Suffix)
	{
		Target = FPaths::Combine(Trash, FString::Printf(TEXT("%s %d.md"), *Base, Suffix));
	}
	IFileManager::Get().MakeDirectory(*Trash, true);
	if (!ProtoBoyDocsProvider::WithRetries([&Target, &Absolute]() { return IFileManager::Get().Move(*Target, *Absolute); }))
	{
		OutError = FText::Format(LOCTEXT("DeleteFailed", "Couldn't move \"{0}\" to {1}."), FText::FromString(RelativePath), GetDeletedDocsLocation());
		return false;
	}

	// Remove the folders this leaves empty (Design/Combat/Weapons/...), up to the docs root. A folder that still has
	// anything in it (another note, an image) stays.
	FString Root = FPaths::ConvertRelativePathToFull(GetDocsRoot());
	FPaths::NormalizeDirectoryName(Root);
	for (FString Dir = FPaths::GetPath(Absolute); Dir.StartsWith(Root + TEXT("/"), ESearchCase::IgnoreCase); Dir = FPaths::GetPath(Dir))
	{
		if (!ProtoBoyDocsProvider::DeleteIfEmpty(Dir))
		{
			break;
		}
	}
	return true;
}

void FProtoBoyFolderDocsProvider::PruneEmptyFolders(const FString& RelativeFolder)
{
	// Same checks as doc paths: the folder must be inside the docs root (and never the root itself).
	FString Absolute;
	FText Ignored;
	if (RelativeFolder.TrimStartAndEnd().IsEmpty() || !ResolvePath(RelativeFolder / TEXT("x.md"), Absolute, Ignored))
	{
		return;
	}
	const FString Dir = FPaths::GetPath(Absolute);
	if (FPaths::DirectoryExists(Dir))
	{
		ProtoBoyDocsProvider::PruneEmpty(Dir);
	}
}

FString FProtoBoyFolderDocsProvider::GetTrashDir() const
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ProtoBoy") / TEXT("DeletedDocs"));
}

FText FProtoBoyFolderDocsProvider::GetDeletedDocsLocation() const
{
	return FText::Format(LOCTEXT("TrashLocation", "{0}"), FText::FromString(GetTrashDir()));
}

FText FProtoBoyLocalDocsProvider::GetDisplayName() const
{
	return FText::Format(LOCTEXT("LocalName", "Project folder ({0})"), FText::FromString(GetDefault<UProtoBoySettings>()->DocsDirectory));
}

FString FProtoBoyLocalDocsProvider::GetDocsRoot() const
{
	FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), GetDefault<UProtoBoySettings>()->DocsDirectory);
	FPaths::NormalizeDirectoryName(Root);
	return Root;
}

#undef LOCTEXT_NAMESPACE
