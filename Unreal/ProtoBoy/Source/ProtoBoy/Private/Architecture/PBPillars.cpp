#include "Architecture/PBPillars.h"

#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/DateTime.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PBModule.h"
#include "PBSettings.h"

#define LOCTEXT_NAMESPACE "ProtoBoyPillars"

FText GetPillarTypeText(EProtoBoyPillarType Type)
{
	return Type == EProtoBoyPillarType::None ? FText::GetEmpty() : StaticEnum<EProtoBoyPillarType>()->GetDisplayNameTextByValue(static_cast<int64>(Type));
}

FText GetPillarTypeDescription(EProtoBoyPillarType Type)
{
	switch (Type)
	{
	case EProtoBoyPillarType::Game: return LOCTEXT("TypeGameTip", "The whole game. Only the root; its doc is the Architecture doc.");
	case EProtoBoyPillarType::Core: return LOCTEXT("TypeCoreTip", "Runtime foundations that features are built on, e.g. save/load, input, game state, AI framework, networking. If several features would break without it, it's Core.");
	case EProtoBoyPillarType::Feature: return LOCTEXT("TypeFeatureTip", "Something the player does or experiences, built on Core, e.g. combat, crafting, dialogue. If removing it removes one thing the player can do, it's a Feature.");
	case EProtoBoyPillarType::Content: return LOCTEXT("TypeContentTip", "Authored design data that features run on, e.g. levels, quests, items, balance tables. If a designer edits it as data, it's Content.");
	case EProtoBoyPillarType::Interface: return LOCTEXT("TypeInterfaceTip", "How the game communicates with the player, e.g. menus, HUD, feedback, onboarding, accessibility options.");
	case EProtoBoyPillarType::Asset: return LOCTEXT("TypeAssetTip", "Produced media and its style, budgets and pipeline, e.g. models, textures, animation, audio, VFX. If an artist or audio designer produces it, it's an Asset.");
	case EProtoBoyPillarType::Tool: return LOCTEXT("TypeToolTip", "Code that helps the team build the game but never ships in it, e.g. editor tools, importers, build scripts, debug utilities.");
	default: return LOCTEXT("TypeNoneTip", "Not classified yet.");
	}
}

TArray<EProtoBoyPillarType> GetPillarTypesForChildren()
{
	return { EProtoBoyPillarType::None, EProtoBoyPillarType::Core, EProtoBoyPillarType::Feature, EProtoBoyPillarType::Content,
		EProtoBoyPillarType::Interface, EProtoBoyPillarType::Asset, EProtoBoyPillarType::Tool };
}

namespace ProtoBoyPillars
{
	/** Same tree, ignoring doc paths (doc bookkeeping isn't an edit). */
	bool SamePillars(const TArray<FProtoBoyPillar>& A, const TArray<FProtoBoyPillar>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 i = 0; i < A.Num(); ++i)
		{
			const FProtoBoyPillar& X = A[i];
			const FProtoBoyPillar& Y = B[i];
			if (X.Id != Y.Id || X.Name != Y.Name || X.ParentId != Y.ParentId || X.Type != Y.Type
				|| X.Description != Y.Description || X.Order != Y.Order)
			{
				return false;
			}
		}
		return true;
	}

	/** A pillar below the root can't be Game. */
	EProtoBoyPillarType SanitizeChildType(EProtoBoyPillarType Type)
	{
		return Type == EProtoBoyPillarType::Game ? EProtoBoyPillarType::None : Type;
	}

	/**
	 * Bring an older pillar file's JSON up to the current format before it's converted (unknown enum names would fail
	 * the whole conversion).
	 * - Types: version 1 had a free-form "kind" string, versions 1-2 a "type" of Domain / Mechanic / Discipline. Names
	 *   of current types carry over (any case); Mechanic becomes Feature and Discipline becomes Asset. Domain has no
	 *   single equivalent, so it becomes None with a warning naming the pillar, as does anything unknown.
	 * - Before version 4, a pillar's "description" was called "vision".
	 */
	void MigrateFile(FJsonObject& Root, int32 Version)
	{
		const TArray<TSharedPtr<FJsonValue>>* Pillars = nullptr;
		if (!Root.TryGetArrayField(TEXT("pillars"), Pillars))
		{
			return;
		}

		if (Version < 4)
		{
			for (const TSharedPtr<FJsonValue>& Value : *Pillars)
			{
				const TSharedPtr<FJsonObject>* Object = nullptr;
				FString Vision;
				if (Value->TryGetObject(Object) && (*Object)->TryGetStringField(TEXT("vision"), Vision))
				{
					(*Object)->RemoveField(TEXT("vision"));
					(*Object)->SetStringField(TEXT("description"), Vision);
				}
			}
		}
		if (Version < 5)
		{
			// The flagged root moves to the front (the first pillar is the root now); dependencies are dropped, since
			// parenting is the dependency graph.
			TArray<TSharedPtr<FJsonValue>> Reordered = *Pillars;
			int32 DroppedDependencies = 0;
			for (int32 Index = 0; Index < Reordered.Num(); ++Index)
			{
				const TSharedPtr<FJsonObject>* Object = nullptr;
				if (!Reordered[Index]->TryGetObject(Object))
				{
					continue;
				}
				const TArray<TSharedPtr<FJsonValue>>* Dependencies = nullptr;
				if ((*Object)->TryGetArrayField(TEXT("dependsOn"), Dependencies))
				{
					DroppedDependencies += Dependencies->Num();
				}
				bool bRoot = false;
				(*Object)->TryGetBoolField(TEXT("bRoot"), bRoot);
				(*Object)->RemoveField(TEXT("dependsOn"));
				(*Object)->RemoveField(TEXT("bRoot"));
				if (bRoot && Index > 0)
				{
					const TSharedPtr<FJsonValue> RootValue = Reordered[Index];
					Reordered.RemoveAt(Index);
					Reordered.Insert(RootValue, 0);
				}
			}
			Root.SetArrayField(TEXT("pillars"), Reordered);
			Root.TryGetArrayField(TEXT("pillars"), Pillars);
			if (DroppedDependencies > 0)
			{
				UE_LOG(LogProtoBoy, Warning, TEXT("Dropped %d pillar dependencies from an older pillar file: parenting is the dependency graph now (a pillar uses its children). Re-parent pillars if a dependency mattered."), DroppedDependencies);
			}
		}
		if (Version >= 3)
		{
			return;
		}

		const UEnum* Enum = StaticEnum<EProtoBoyPillarType>();
		const TCHAR* OldField = Version < 2 ? TEXT("kind") : TEXT("type");
		for (const TSharedPtr<FJsonValue>& Value : *Pillars)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (!Value->TryGetObject(Object))
			{
				continue;
			}
			FString Old;
			(*Object)->TryGetStringField(OldField, Old);
			(*Object)->RemoveField(TEXT("kind"));
			Old.TrimStartAndEndInline();

			FString New;
			if (Old.Equals(TEXT("Mechanic"), ESearchCase::IgnoreCase))
			{
				New = TEXT("Feature");
			}
			else if (Old.Equals(TEXT("Discipline"), ESearchCase::IgnoreCase))
			{
				New = TEXT("Asset");
			}
			else
			{
				for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index) // The last entry is the generated _MAX.
				{
					if (Enum->GetNameStringByIndex(Index).Equals(Old, ESearchCase::IgnoreCase))
					{
						New = Enum->GetNameStringByIndex(Index);
						break;
					}
				}
			}
			if (New.IsEmpty())
			{
				New = TEXT("None");
				if (!Old.IsEmpty())
				{
					FString Name;
					(*Object)->TryGetStringField(TEXT("name"), Name);
					UE_LOG(LogProtoBoy, Warning, TEXT("Pillar '%s' was '%s', which has no matching pillar type; pick one in Architecture > Pillars."), *Name, *Old);
				}
			}
			(*Object)->SetStringField(TEXT("type"), New);
		}
	}

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
	// The file may have changed under us (e.g. a pull), so old history no longer applies.
	UndoStack.Reset();
	RedoStack.Reset();

	const FString Path = GetFilePath();
	if (!FPaths::FileExists(Path))
	{
		EnsureRoot();
		CommittedPillars = Tree.Pillars;
		OnChanged.Broadcast();
		return true;
	}

	// Read the JSON first: the version decides whether it needs migrating before it's converted.
	FString Json;
	TSharedPtr<FJsonObject> Object;
	FProtoBoyPillarTree Loaded;
	int32 FileVersion = 1;
	const bool bParsed = FFileHelper::LoadFileToString(Json, *Path)
		&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) && Object.IsValid();
	if (bParsed)
	{
		Object->TryGetNumberField(TEXT("version"), FileVersion);
	}
	if (bParsed && FileVersion > FProtoBoyPillarTree::CurrentVersion)
	{
		LastError = FText::Format(LOCTEXT("TooNew", "{0} was written by a newer ProtoBoy (version {1})."), FText::FromString(Path), FText::AsNumber(FileVersion));
		OutError = LastError;
		EnsureRoot();
		CommittedPillars = Tree.Pillars;
		OnChanged.Broadcast();
		return false;
	}
	if (bParsed && FileVersion < FProtoBoyPillarTree::CurrentVersion)
	{
		ProtoBoyPillars::MigrateFile(*Object, FileVersion);
	}
	if (!bParsed || !FJsonObjectConverter::JsonObjectToUStruct(Object.ToSharedRef(), &Loaded))
	{
		LastError = FText::Format(LOCTEXT("ParseFailed", "Couldn't read the pillar file {0}. Fix it or remove it; ProtoBoy won't overwrite it."), FText::FromString(Path));
		OutError = LastError;
		UE_LOG(LogProtoBoy, Warning, TEXT("%s"), *LastError.ToString());
		EnsureRoot();
		CommittedPillars = Tree.Pillars;
		OnChanged.Broadcast();
		return false;
	}
	Loaded.Version = FProtoBoyPillarTree::CurrentVersion;

	Tree = MoveTemp(Loaded);
	EnsureRoot();
	CommittedPillars = Tree.Pillars;
	OnChanged.Broadcast();
	return true;
}

void FProtoBoyPillarService::CommitEdit(const FText& Description)
{
	if (!ProtoBoyPillars::SamePillars(CommittedPillars, Tree.Pillars))
	{
		UndoStack.Add({ CommittedPillars, Description });
		if (UndoStack.Num() > MaxHistory)
		{
			UndoStack.RemoveAt(0);
		}
		RedoStack.Reset();
	}
	SaveAndNotify();
}

void FProtoBoyPillarService::Undo()
{
	if (!CanUndo())
	{
		return;
	}
	const FHistoryEntry Entry = UndoStack.Pop();
	RedoStack.Add({ Tree.Pillars, Entry.Description });
	RestorePillars(Entry.Pillars);
	SaveAndNotify();
}

void FProtoBoyPillarService::Redo()
{
	if (!CanRedo())
	{
		return;
	}
	const FHistoryEntry Entry = RedoStack.Pop();
	UndoStack.Add({ Tree.Pillars, Entry.Description });
	RestorePillars(Entry.Pillars);
	SaveAndNotify();
}

void FProtoBoyPillarService::RestorePillars(const TArray<FProtoBoyPillar>& Pillars)
{
	// Pillars the restored tree doesn't have are removed like any other removal, so their docs get archived.
	TArray<FString> Removed;
	for (const FProtoBoyPillar& Current : Tree.Pillars)
	{
		if (!Pillars.ContainsByPredicate([&Current](const FProtoBoyPillar& P) { return P.Id == Current.Id; }))
		{
			Removed.Add(Current.Id);
		}
	}
	RemoveAndArchive(Removed);

	TArray<FProtoBoyPillar> Restored = Pillars;
	for (FProtoBoyPillar& Pillar : Restored)
	{
		// Keep doc paths generated since the snapshot was taken.
		if (const FProtoBoyPillar* Current = Find(Pillar.Id); Current && !Current->DocPath.IsEmpty())
		{
			Pillar.DocPath = Current->DocPath;
		}
		// A pillar that comes back is no longer archived: its doc is still on disk and will be updated again.
		if (const FProtoBoyArchivedPillar* Archived = Tree.Archived.FindByPredicate([&Pillar](const FProtoBoyArchivedPillar& A) { return A.Id == Pillar.Id; }))
		{
			if (Pillar.DocPath.IsEmpty())
			{
				Pillar.DocPath = Archived->DocPath;
			}
			const FString Id = Pillar.Id;
			Tree.Archived.RemoveAll([&Id](const FProtoBoyArchivedPillar& A) { return A.Id == Id; });
		}
	}
	Tree.Pillars = MoveTemp(Restored);
	EnsureRoot();
}

void FProtoBoyPillarService::SaveAndNotify()
{
	CommittedPillars = Tree.Pillars;

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

void FProtoBoyPillarService::EnsureRoot()
{
	const FString ProjectName = FApp::GetProjectName();
	const FString RootName = ProjectName.IsEmpty() ? FString(TEXT("Game")) : ProjectName;

	// The first pillar is the root. A file without one (from before the root existed, or emptied by hand) gets a new
	// root in front; only the root is ever saved with type Game, so that's how it's recognised.
	if (Tree.Pillars.IsEmpty() || Tree.Pillars[0].Type != EProtoBoyPillarType::Game)
	{
		FProtoBoyPillar NewRoot;
		NewRoot.Id = MakeUniqueId(RootName);
		Tree.Pillars.Insert(NewRoot, 0);
	}
	FProtoBoyPillar& Root = Tree.Pillars[0];
	Root.Name = RootName;
	Root.Type = EProtoBoyPillarType::Game;
	Root.ParentId.Reset();
	Root.Order = 0;
	const FString RootId = Root.Id;

	for (int32 Index = 1; Index < Tree.Pillars.Num(); ++Index)
	{
		Tree.Pillars[Index].Type = ProtoBoyPillars::SanitizeChildType(Tree.Pillars[Index].Type);
	}

	// Everything else hangs below the root. Top-level pillars from older files, and pillars whose parent is missing
	// (e.g. a hand-edited file), move under it after its existing children.
	bool bMoved = false;
	for (FProtoBoyPillar& Pillar : Tree.Pillars)
	{
		if (Pillar.Id != RootId && (Pillar.ParentId.IsEmpty() || Pillar.ParentId == Pillar.Id || !Find(Pillar.ParentId)))
		{
			Pillar.ParentId = RootId;
			Pillar.Order += 1000000;
			bMoved = true;
		}
	}
	if (bMoved)
	{
		Renumber(RootId);
	}
}

const FProtoBoyPillar& FProtoBoyPillarService::GetRoot() const
{
	// EnsureRoot runs on every load, and nothing removes or reorders the first pillar, so there's always one.
	check(!Tree.Pillars.IsEmpty());
	return Tree.Pillars[0];
}

FString FProtoBoyPillarService::ResolveParent(const FString& ParentId) const
{
	return ParentId.IsEmpty() ? GetRootId() : ParentId;
}

TArray<const FProtoBoyPillar*> FProtoBoyPillarService::GetChildren(const FString& ParentId) const
{
	TArray<const FProtoBoyPillar*> Children;
	for (const FProtoBoyPillar& Pillar : Tree.Pillars)
	{
		if (Pillar.ParentId == ParentId)
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
		// Every path starts at the root, so it adds nothing.
		if (!IsRoot(Pillar->Id))
		{
			Names.Add(Pillar->Name);
		}
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
	Pillar.ParentId = Find(ParentId) ? ParentId : GetRootId();
	Pillar.Order = GetChildren(Pillar.ParentId).Num();
	Tree.Pillars.Add(Pillar);
	CommitEdit(LOCTEXT("UndoAdd", "Add Pillar"));
	return Pillar.Id;
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

	// The root is the game: it keeps the project's name and type.
	const bool bRoot = IsRoot(Pillar->Id);
	const EProtoBoyPillarType Type = bRoot ? EProtoBoyPillarType::Game : ProtoBoyPillars::SanitizeChildType(Edited.Type);
	if (!bRoot)
	{
		Pillar->Name = Name;
	}
	Pillar->Type = Type;
	Pillar->Description = Edited.Description.TrimStartAndEnd();
	CommitEdit(FText::Format(LOCTEXT("UndoEdit", "Edit {0}"), FText::FromString(Pillar->Name)));
	return true;
}

bool FProtoBoyPillarService::SetParent(const FString& Id, const FString& NewParentId, FText& OutError)
{
	const FProtoBoyPillar* Pillar = Find(Id);
	if (Pillar && !IsRoot(Id) && Pillar->ParentId == ResolveParent(NewParentId))
	{
		return true;
	}
	return MovePillar(Id, NewParentId, INDEX_NONE, OutError);
}

bool FProtoBoyPillarService::MovePillar(const FString& Id, const FString& InNewParentId, int32 Index, FText& OutError)
{
	FProtoBoyPillar* Pillar = FindMutable(Id);
	if (!Pillar)
	{
		OutError = LOCTEXT("NoPillarMove", "That pillar no longer exists.");
		return false;
	}
	if (IsRoot(Id))
	{
		OutError = LOCTEXT("MoveRoot", "The project is the root of the pillar tree; it can't be moved.");
		return false;
	}
	const FString NewParentId = ResolveParent(InNewParentId);
	if (!Find(NewParentId))
	{
		OutError = LOCTEXT("NoParent", "That parent pillar no longer exists.");
		return false;
	}
	if (IsInSubtree(NewParentId, Id))
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
	CommitEdit(FText::Format(LOCTEXT("UndoMove", "Move {0}"), FText::FromString(Pillar->Name)));
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
	CommitEdit(FText::Format(LOCTEXT("UndoReorder", "Reorder {0}"), FText::FromString(Pillar->Name)));
}

void FProtoBoyPillarService::RemovePillar(const FString& Id)
{
	const FProtoBoyPillar* Removing = Find(Id);
	if (!Removing || IsRoot(Id))
	{
		return;
	}
	const FString ParentId = Removing->ParentId;
	const FString RemovedName = Removing->Name;

	TArray<FString> Removed;
	for (const FProtoBoyPillar& Pillar : Tree.Pillars)
	{
		if (IsInSubtree(Pillar.Id, Id))
		{
			Removed.Add(Pillar.Id);
		}
	}
	RemoveAndArchive(Removed);
	Renumber(ParentId);
	CommitEdit(FText::Format(LOCTEXT("UndoRemove", "Remove {0}"), FText::FromString(RemovedName)));
}

void FProtoBoyPillarService::ClearPillars()
{
	TArray<FString> Removed;
	for (int32 Index = 1; Index < Tree.Pillars.Num(); ++Index)
	{
		Removed.Add(Tree.Pillars[Index].Id);
	}
	if (Removed.IsEmpty())
	{
		return;
	}
	RemoveAndArchive(Removed);
	CommitEdit(LOCTEXT("UndoClear", "Clear Pillars"));
}

void FProtoBoyPillarService::ReplacePillars(const TArray<FProtoBoyPillar>& Pillars)
{
	TArray<FString> Removed;
	for (int32 Index = 1; Index < Tree.Pillars.Num(); ++Index)
	{
		Removed.Add(Tree.Pillars[Index].Id);
	}
	RemoveAndArchive(Removed);

	// New ids first (each added right away so later ones see it), then parents through the map. The template's first
	// pillar is its root and stands for this tree's root.
	TMap<FString, FString> NewIds;
	for (int32 SourceIndex = 0; SourceIndex < Pillars.Num(); ++SourceIndex)
	{
		const FProtoBoyPillar& Source = Pillars[SourceIndex];
		if (SourceIndex == 0)
		{
			NewIds.Add(Source.Id, GetRootId());
			continue;
		}
		FProtoBoyPillar Pillar;
		Pillar.Name = Source.Name.TrimStartAndEnd().IsEmpty() ? FString(TEXT("New Pillar")) : Source.Name.TrimStartAndEnd();
		Pillar.Id = MakeUniqueId(Source.Id.IsEmpty() ? Pillar.Name : Source.Id);
		Pillar.Type = ProtoBoyPillars::SanitizeChildType(Source.Type);
		Pillar.Description = Source.Description;
		Pillar.Order = Source.Order;
		NewIds.Add(Source.Id, Pillar.Id);
		Tree.Pillars.Add(Pillar);
	}
	for (int32 SourceIndex = 1; SourceIndex < Pillars.Num(); ++SourceIndex)
	{
		const FProtoBoyPillar& Source = Pillars[SourceIndex];
		const FString* NewId = NewIds.Find(Source.Id);
		FProtoBoyPillar* Pillar = NewId ? FindMutable(*NewId) : nullptr;
		if (!Pillar)
		{
			continue;
		}
		const FString* NewParent = NewIds.Find(Source.ParentId);
		Pillar->ParentId = NewParent ? *NewParent : GetRootId();
	}

	// A template's Order may have gaps or ties; make each sibling list 0..N-1 in that order.
	TSet<FString> Parents;
	for (int32 Index = 1; Index < Tree.Pillars.Num(); ++Index)
	{
		Parents.Add(Tree.Pillars[Index].ParentId);
	}
	for (const FString& Parent : Parents)
	{
		Renumber(Parent);
	}
	CommitEdit(LOCTEXT("UndoReplace", "Apply Template"));
}

void FProtoBoyPillarService::RemoveAndArchive(const TArray<FString>& Removed)
{
	// Pillars with a doc are remembered so the next doc generation marks the doc archived (never deleted).
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
