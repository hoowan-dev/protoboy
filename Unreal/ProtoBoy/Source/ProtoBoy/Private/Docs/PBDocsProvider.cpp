#include "Docs/PBDocsProvider.h"

#include "Docs/PBMarkdown.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PBSettings.h"

#define LOCTEXT_NAMESPACE "ProtoBoyDocsProvider"

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
	if (!FFileHelper::SaveStringToFile(Text, *Absolute, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FText::Format(LOCTEXT("WriteFailed", "Couldn't write \"{0}\"."), FText::FromString(RelativePath));
		return false;
	}
	return true;
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
