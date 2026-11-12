#include "Planning/PBPillars.h"

#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PBModule.h"
#include "PBSettings.h"

#define LOCTEXT_NAMESPACE "ProtoBoyPillars"

namespace ProtoBoyPillars
{
	FString Slugify(const FString& Name)
	{
		FString Slug;
		bool bDash = false;
		for (const TCHAR C : Name)
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
		return Slug.IsEmpty() ? FString(TEXT("pillar")) : Slug.Left(48);
	}
}

FProtoBoyPillarService::FProtoBoyPillarService()
{
	FText Ignored;
	Reload(Ignored);
}

FString FProtoBoyPillarService::GetFilePath()
{
	FString Relative = GetDefault<UProtoBoySettings>()->PillarsFilePath.TrimStartAndEnd();
	if (Relative.IsEmpty())
	{
		Relative = TEXT("ProtoBoy/Pillars.json");
	}
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), Relative);
}

bool FProtoBoyPillarService::Reload(FText& OutError)
{
	Tree = FProtoBoyPillarTree();
	LastError = FText::GetEmpty();

	const FString Path = GetFilePath();
	if (!FPaths::FileExists(Path))
	{
		OnChanged.Broadcast();
		return true;
	}

	FString Json;
	FProtoBoyPillarTree Loaded;
	if (!FFileHelper::LoadFileToString(Json, *Path) || !FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Loaded))
	{
		LastError = FText::Format(LOCTEXT("ParseFailed", "Couldn't read the pillar file {0}. Fix it or remove it; ProtoBoy won't overwrite it."), FText::FromString(Path));
		OutError = LastError;
		UE_LOG(LogProtoBoy, Warning, TEXT("%s"), *LastError.ToString());
		OnChanged.Broadcast();
		return false;
	}
	if (Loaded.Version > FProtoBoyPillarTree::CurrentVersion)
	{
		LastError = FText::Format(LOCTEXT("TooNew", "{0} was written by a newer ProtoBoy (version {1})."), FText::FromString(Path), FText::AsNumber(Loaded.Version));
		OutError = LastError;
		OnChanged.Broadcast();
		return false;
	}

	Tree = MoveTemp(Loaded);
	OnChanged.Broadcast();
	return true;
}

void FProtoBoyPillarService::SaveAndNotify()
{
	// Don't clobber a file we couldn't read.
	if (!LastError.IsEmpty() && FPaths::FileExists(GetFilePath()))
	{
		OnChanged.Broadcast();
		return;
	}

	FString Json;
	FJsonObjectConverter::UStructToJsonObjectString(Tree, Json);

	const FString Path = GetFilePath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	if (!FFileHelper::SaveStringToFile(Json, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		LastError = FText::Format(LOCTEXT("SaveFailed", "Couldn't write {0}."), FText::FromString(Path));
		UE_LOG(LogProtoBoy, Error, TEXT("%s"), *LastError.ToString());
	}
	OnChanged.Broadcast();
}

const FProtoBoyPillar* FProtoBoyPillarService::Find(const FString& Id) const
{
	return Tree.Pillars.FindByPredicate([&Id](const FProtoBoyPillar& P) { return P.Id == Id; });
}

FProtoBoyPillar* FProtoBoyPillarService::FindMutable(const FString& Id)
{
	return Tree.Pillars.FindByPredicate([&Id](const FProtoBoyPillar& P) { return P.Id == Id; });
}

TArray<const FProtoBoyPillar*> FProtoBoyPillarService::GetChildren(const FString& ParentId) const
{
	TArray<const FProtoBoyPillar*> Children;
	for (const FProtoBoyPillar& Pillar : Tree.Pillars)
	{
		// Pillars whose parent is missing (e.g. a hand-edited file) show at the top level.
		const bool bParentMissing = !Pillar.ParentId.IsEmpty() && !Find(Pillar.ParentId);
		if (Pillar.ParentId == ParentId || (ParentId.IsEmpty() && bParentMissing))
		{
			Children.Add(&Pillar);
		}
	}
	Children.Sort([](const FProtoBoyPillar& A, const FProtoBoyPillar& B) { return A.Order < B.Order; });
	return Children;
}

TArray<const FProtoBoyPillar*> FProtoBoyPillarService::GetAncestors(const FString& Id) const
{
	TArray<const FProtoBoyPillar*> Ancestors;
	const FProtoBoyPillar* Current = Find(Id);
	TSet<FString> Seen;
	while (Current && !Current->ParentId.IsEmpty() && !Seen.Contains(Current->ParentId))
	{
		Seen.Add(Current->ParentId);
		Current = Find(Current->ParentId);
		if (Current)
		{
			Ancestors.Insert(Current, 0);
		}
	}
	return Ancestors;
}

bool FProtoBoyPillarService::IsInSubtree(const FString& Id, const FString& Ancestor) const
{
	if (Id == Ancestor)
	{
		return true;
	}
	for (const FProtoBoyPillar* Pillar : GetAncestors(Id))
	{
		if (Pillar->Id == Ancestor)
		{
			return true;
		}
	}
	return false;
}

FString FProtoBoyPillarService::GetDisplayPath(const FString& Id) const
{
	TArray<FString> Names;
	for (const FProtoBoyPillar* Pillar : GetAncestors(Id))
	{
		Names.Add(Pillar->Name);
	}
	if (const FProtoBoyPillar* Pillar = Find(Id))
	{
		Names.Add(Pillar->Name);
	}
	return FString::Join(Names, TEXT(" / "));
}

FString FProtoBoyPillarService::MakeUniqueId(const FString& Name) const
{
	const FString Base = ProtoBoyPillars::Slugify(Name);
	FString Id = Base;
	for (int32 Suffix = 2; Find(Id) || Tree.Archived.ContainsByPredicate([&Id](const FProtoBoyArchivedPillar& A) { return A.Id == Id; }); ++Suffix)
	{
		Id = FString::Printf(TEXT("%s-%d"), *Base, Suffix);
	}
	return Id;
}

void FProtoBoyPillarService::Renumber(const FString& ParentId)
{
	int32 Order = 0;
	for (const FProtoBoyPillar* Child : GetChildren(ParentId))
	{
		FindMutable(Child->Id)->Order = Order++;
	}
}

FString FProtoBoyPillarService::AddPillar(const FString& Name, const FString& ParentId)
{
	FProtoBoyPillar Pillar;
	Pillar.Name = Name.TrimStartAndEnd().IsEmpty() ? FString(TEXT("New Pillar")) : Name.TrimStartAndEnd();
	Pillar.Id = MakeUniqueId(Pillar.Name);
	Pillar.ParentId = Find(ParentId) ? ParentId : FString();
	Pillar.Order = GetChildren(Pillar.ParentId).Num();
	Tree.Pillars.Add(Pillar);
	SaveAndNotify();
	return Pillar.Id;
}

void FProtoBoyPillarService::AddStarterPillars()
{
	struct FStarter { const TCHAR* Name; const TCHAR* Kind; const TCHAR* Vision; };
	const FStarter Starters[] = {
		{ TEXT("Gameplay"), TEXT("Domain"), TEXT("The core loop, mechanics and rules the player interacts with.") },
		{ TEXT("Animation"), TEXT("Discipline"), TEXT("How characters and objects move, and how that supports game feel.") },
		{ TEXT("Art"), TEXT("Discipline"), TEXT("The visual style, assets and their budgets.") },
		{ TEXT("Audio"), TEXT("Discipline"), TEXT("Music, sound effects and how they react to play.") },
		{ TEXT("Content"), TEXT("Domain"), TEXT("Levels, encounters, items and other authored content.") },
		{ TEXT("UI"), TEXT("Domain"), TEXT("Menus, HUD and how the game communicates with the player.") },
	};

	for (const FStarter& Starter : Starters)
	{
		const bool bExists = Tree.Pillars.ContainsByPredicate([&Starter](const FProtoBoyPillar& P)
		{
			return P.ParentId.IsEmpty() && P.Name.Equals(Starter.Name, ESearchCase::IgnoreCase);
		});
		if (bExists)
		{
			continue;
		}

		FProtoBoyPillar Pillar;
		Pillar.Name = Starter.Name;
		Pillar.Id = MakeUniqueId(Pillar.Name);
		Pillar.Kind = Starter.Kind;
		Pillar.Vision = Starter.Vision;
		Pillar.Order = GetChildren(FString()).Num();
		Tree.Pillars.Add(Pillar);
	}
	SaveAndNotify();
}

bool FProtoBoyPillarService::UpdatePillar(const FProtoBoyPillar& Edited, FText& OutError)
{
	FProtoBoyPillar* Pillar = FindMutable(Edited.Id);
	if (!Pillar)
	{
		OutError = LOCTEXT("NoPillar", "That pillar no longer exists.");
		return false;
	}
	const FString Name = Edited.Name.TrimStartAndEnd();
	if (Name.IsEmpty())
	{
		OutError = LOCTEXT("EmptyName", "A pillar needs a name.");
		return false;
	}

	Pillar->Name = Name;
	Pillar->Kind = Edited.Kind.TrimStartAndEnd();
	Pillar->Vision = Edited.Vision.TrimStartAndEnd();
	Pillar->DependsOn.Reset();
	for (const FString& Dependency : Edited.DependsOn)
	{
		if (Dependency != Pillar->Id && Find(Dependency))
		{
			Pillar->DependsOn.AddUnique(Dependency);
		}
	}
	SaveAndNotify();
	return true;
}

bool FProtoBoyPillarService::SetParent(const FString& Id, const FString& NewParentId, FText& OutError)
{
	const FProtoBoyPillar* Pillar = Find(Id);
	if (Pillar && Pillar->ParentId == NewParentId)
	{
		return true;
	}
	return MovePillar(Id, NewParentId, INDEX_NONE, OutError);
}

bool FProtoBoyPillarService::MovePillar(const FString& Id, const FString& NewParentId, int32 Index, FText& OutError)
{
	FProtoBoyPillar* Pillar = FindMutable(Id);
	if (!Pillar)
	{
		OutError = LOCTEXT("NoPillarMove", "That pillar no longer exists.");
		return false;
	}
	if (!NewParentId.IsEmpty() && (!Find(NewParentId) || IsInSubtree(NewParentId, Id)))
	{
		OutError = LOCTEXT("BadParent", "A pillar can't be moved under itself or one of its own children.");
		return false;
	}

	const FString OldParent = Pillar->ParentId;
	TArray<const FProtoBoyPillar*> Siblings = GetChildren(NewParentId);
	Siblings.Remove(Pillar);
	Siblings.Insert(Pillar, Siblings.IsValidIndex(Index) ? Index : Siblings.Num());

	Pillar->ParentId = NewParentId;
	for (int32 i = 0; i < Siblings.Num(); ++i)
	{
		FindMutable(Siblings[i]->Id)->Order = i;
	}
	if (OldParent != NewParentId)
	{
		Renumber(OldParent);
	}
	SaveAndNotify();
	return true;
}

void FProtoBoyPillarService::MoveAmongSiblings(const FString& Id, int32 Delta)
{
	const FProtoBoyPillar* Pillar = Find(Id);
	if (!Pillar)
	{
		return;
	}
	const FString ParentId = Pillar->ParentId;
	TArray<const FProtoBoyPillar*> Siblings = GetChildren(ParentId);
	const int32 Index = Siblings.IndexOfByKey(Pillar);
	const int32 Target = Index + Delta;
	if (Index == INDEX_NONE || !Siblings.IsValidIndex(Target))
	{
		return;
	}

	Siblings.Swap(Index, Target);
	for (int32 i = 0; i < Siblings.Num(); ++i)
	{
		FindMutable(Siblings[i]->Id)->Order = i;
	}
	SaveAndNotify();
}

void FProtoBoyPillarService::RemovePillar(const FString& Id)
{
	const FProtoBoyPillar* Root = Find(Id);
	if (!Root)
	{
		return;
	}
	const FString ParentId = Root->ParentId;

	TArray<FString> Removed;
	for (const FProtoBoyPillar& Pillar : Tree.Pillars)
	{
		if (IsInSubtree(Pillar.Id, Id))
		{
			Removed.Add(Pillar.Id);
		}
	}

	const FString Now = FDateTime::UtcNow().ToIso8601();
	for (const FString& RemovedId : Removed)
	{
		const FProtoBoyPillar* Pillar = Find(RemovedId);
		if (Pillar && !Pillar->DocPath.IsEmpty())
		{
			FProtoBoyArchivedPillar Archived;
			Archived.Id = Pillar->Id;
			Archived.Name = Pillar->Name;
			Archived.DocPath = Pillar->DocPath;
			Archived.RemovedUtc = Now;
			Tree.Archived.Add(Archived);
		}
	}

	Tree.Pillars.RemoveAll([&Removed](const FProtoBoyPillar& P) { return Removed.Contains(P.Id); });
	for (FProtoBoyPillar& Pillar : Tree.Pillars)
	{
		Pillar.DependsOn.RemoveAll([&Removed](const FString& Dependency) { return Removed.Contains(Dependency); });
	}
	Renumber(ParentId);
	SaveAndNotify();
}

void FProtoBoyPillarService::SetDocPath(const FString& Id, const FString& DocPath)
{
	if (FProtoBoyPillar* Pillar = FindMutable(Id))
	{
		if (Pillar->DocPath != DocPath)
		{
			Pillar->DocPath = DocPath;
			SaveAndNotify();
		}
	}
}

void FProtoBoyPillarService::ClearArchived(const FString& Id)
{
	if (Tree.Archived.RemoveAll([&Id](const FProtoBoyArchivedPillar& A) { return A.Id == Id; }) > 0)
	{
		SaveAndNotify();
	}
}

#undef LOCTEXT_NAMESPACE
