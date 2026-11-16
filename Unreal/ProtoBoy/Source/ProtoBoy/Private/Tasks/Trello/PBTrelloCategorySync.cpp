// Task categories on the board: "To Do - <Category>" lists and category labels (Tasks > Sync with Trello).
// Part of FProtoBoyTrelloClient; the plan is FProtoBoyTrelloCategorySync.
#include "Tasks/Trello/PBTrelloCategorySync.h"

#include "PBSettings.h"
#include "Tasks/PBTaskProvider.h"
#include "Tasks/Trello/PBTrelloClient.h"
#include "Tasks/Trello/PBTrelloJson.h"

#define LOCTEXT_NAMESPACE "ProtoBoyTrelloCategorySync"

namespace ProtoBoyTrelloCategorySync
{
	/** A list on the board, with its open cards. */
	struct FList
	{
		FString Id;
		FString Name;
		double Pos = 0.0;
		TArray<TSharedPtr<FJsonObject>> Cards;
	};

	/** Parse "lists?cards=open" into lists in board order. */
	TArray<FList> ParseLists(const TArray<TSharedPtr<FJsonValue>>& Array)
	{
		TArray<FList> Lists;
		for (const TSharedPtr<FJsonValue>& Value : Array)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (!Value->TryGetObject(Object))
			{
				continue;
			}
			FList& List = Lists.AddDefaulted_GetRef();
			List.Id = ProtoBoyTrelloClient::GetString(*Object, TEXT("id"));
			List.Name = ProtoBoyTrelloClient::GetString(*Object, TEXT("name"));
			(*Object)->TryGetNumberField(TEXT("pos"), List.Pos);
			const TArray<TSharedPtr<FJsonValue>>* Cards = nullptr;
			if ((*Object)->TryGetArrayField(TEXT("cards"), Cards))
			{
				for (const TSharedPtr<FJsonValue>& CardValue : *Cards)
				{
					const TSharedPtr<FJsonObject>* Card = nullptr;
					if (CardValue->TryGetObject(Card))
					{
						List.Cards.Add(*Card);
					}
				}
			}
		}
		Lists.Sort([](const FList& A, const FList& B) { return A.Pos < B.Pos; });
		return Lists;
	}

	/** The colors new category labels cycle through. */
	const TCHAR* const LabelColors[] = { TEXT("green"), TEXT("yellow"), TEXT("orange"), TEXT("red"), TEXT("purple"), TEXT("blue"), TEXT("sky"), TEXT("lime"), TEXT("pink"), TEXT("black") };
}

FString FProtoBoyTrelloClient::GetCategoryListName(const FString& Category)
{
	return FString::Printf(TEXT("To Do - %s"), *Category.TrimStartAndEnd());
}

bool FProtoBoyTrelloClient::ParseCategoryListName(const FString& ListName, FString& OutCategory)
{
	FString Rest = ListName.TrimStartAndEnd();
	if (!Rest.StartsWith(TEXT("To Do"), ESearchCase::IgnoreCase))
	{
		return false;
	}
	Rest = Rest.RightChop(5).TrimStart();
	// "-" or an en dash (some apps turn one into the other).
	if (!Rest.StartsWith(TEXT("-")) && !Rest.StartsWith(TEXT("–")))
	{
		return false;
	}
	OutCategory = Rest.RightChop(1).TrimStartAndEnd();
	return !OutCategory.IsEmpty();
}

void FProtoBoyTrelloClient::RememberCategoryIds(const TMap<FString, FString>& ListIds, const TMap<FString, FString>& LabelIds)
{
	UProtoBoySettings* Settings = GetMutableDefault<UProtoBoySettings>();
	if (Settings->TrelloCategoryListIds.OrderIndependentCompareEqual(ListIds) && Settings->TrelloCategoryLabelIds.OrderIndependentCompareEqual(LabelIds))
	{
		return;
	}
	Settings->TrelloCategoryListIds = ListIds;
	Settings->TrelloCategoryLabelIds = LabelIds;
	Settings->TryUpdateDefaultConfigFile();
}

void FProtoBoyTrelloClient::PlanCategorySync(const TArray<FProtoBoyTaskCategory>& InCategories, FCategorySyncCallback OnDone)
{
	using FResult = TProtoBoyTaskResult<FProtoBoyTrelloCategorySync>;
	FText WhyNot;
	if (!IsReady(WhyNot))
	{
		OnDone(FResult::Failure(WhyNot));
		return;
	}
	const FString BoardId = GetDefault<UProtoBoySettings>()->TrelloBoardId;
	if (!IsSafeId(BoardId))
	{
		OnDone(FResult::Failure(LOCTEXT("SyncBadBoard", "Invalid board id.")));
		return;
	}

	// Each category once (by doc), with a usable title; "Uncategorized" can't be a category (FProtoBoyDocMap refuses it).
	TArray<FProtoBoyTaskCategory> Categories;
	for (const FProtoBoyTaskCategory& Category : InCategories)
	{
		const FString Title = Category.Title.TrimStartAndEnd();
		if (!Category.DocId.IsEmpty() && !Title.IsEmpty() && !ProtoBoyTasks::IsReservedCategoryName(Title)
			&& !Categories.ContainsByPredicate([&Category](const FProtoBoyTaskCategory& C) { return C.DocId == Category.DocId; }))
		{
			Categories.Add({ Category.DocId, Title });
		}
	}

	// Step 1: the board's labels. Step 2: its lists with their cards, then the plan.
	const TSharedRef<TArray<FProtoBoyTrelloItem>> BoardLabels = MakeShared<TArray<FProtoBoyTrelloItem>>();
	TFunction<void()> FetchLists = [this, BoardId, Categories, BoardLabels, OnDone]()
	{
		const FString Path = FString::Printf(TEXT("boards/%s/lists?filter=open&fields=name,pos&cards=open&card_fields=name,desc,idList,shortLink,shortUrl,labels,due,dateLastActivity"), *BoardId);
		Call(TEXT("GET"), Path, nullptr, [this, Categories, BoardLabels, OnDone](bool bOk, const FText& Error, const TSharedPtr<FJsonValue>& Json)
		{
			const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
			if (!bOk || !Json->TryGetArray(Array))
			{
				OnDone(FResult::Failure(bOk ? LOCTEXT("SyncBadLists", "Trello sent an unexpected list of lists.") : Error));
				return;
			}
			const TArray<ProtoBoyTrelloCategorySync::FList> Lists = ProtoBoyTrelloCategorySync::ParseLists(*Array);
			TMap<FString, FString> ListNames;
			for (const ProtoBoyTrelloCategorySync::FList& List : Lists)
			{
				ListNames.Add(List.Id, List.Name);
			}
			OnDone(FResult::Success(BuildCategoryPlan(Categories, Lists, *BoardLabels, ListNames)));
		});
	};

	Call(TEXT("GET"), FString::Printf(TEXT("boards/%s/labels?fields=name&limit=1000"), *BoardId), nullptr,
		[BoardLabels, FetchLists, OnDone](bool bOk, const FText& Error, const TSharedPtr<FJsonValue>& Json)
		{
			const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
			if (!bOk || !Json->TryGetArray(Array))
			{
				OnDone(FResult::Failure(bOk ? LOCTEXT("SyncBadLabels", "Trello sent an unexpected list of labels.") : Error));
				return;
			}
			for (const TSharedPtr<FJsonValue>& Value : *Array)
			{
				const TSharedPtr<FJsonObject>* Label = nullptr;
				if (Value->TryGetObject(Label))
				{
					BoardLabels->Add({ ProtoBoyTrelloClient::GetString(*Label, TEXT("id")), ProtoBoyTrelloClient::GetString(*Label, TEXT("name")) });
				}
			}
			FetchLists();
		});
}

FProtoBoyTrelloCategorySync FProtoBoyTrelloClient::BuildCategoryPlan(const TArray<FProtoBoyTaskCategory>& Categories,
	const TArray<ProtoBoyTrelloCategorySync::FList>& Lists, const TArray<FProtoBoyTrelloItem>& Labels, const TMap<FString, FString>& ListNames) const
{
	using FPlan = FProtoBoyTrelloCategorySync;
	const UProtoBoySettings* Settings = GetDefault<UProtoBoySettings>();
	FPlan Plan;

	// Lists ProtoBoy maps statuses to are never category lists, whatever they're called.
	auto IsStatusList = [Settings](const FString& Id)
	{
		return Id == Settings->TrelloInProgressListId || Id == Settings->TrelloDoneListId;
	};

	// In Progress and Done, for the Tasks page.
	for (const ProtoBoyTrelloCategorySync::FList& List : Lists)
	{
		TArray<FProtoBoyTask>* Into = List.Id == Settings->TrelloInProgressListId ? &Plan.InProgressTasks
			: List.Id == Settings->TrelloDoneListId ? &Plan.DoneTasks : nullptr;
		if (Into)
		{
			for (const TSharedPtr<FJsonObject>& Card : List.Cards)
			{
				Into->Add(ParseCard(Card, ListNames));
			}
		}
	}
	// ISO 8601 strings sort by time.
	Plan.DoneTasks.Sort([](const FProtoBoyTask& A, const FProtoBoyTask& B) { return A.LastActivityUtc > B.LastActivityUtc; });

	// --- Lists: each category's by its remembered id, else by name; Uncategorized's by name. ---
	TSet<int32> Claimed;
	int32 LastToDoIndex = INDEX_NONE;
	auto FindByName = [&Lists, &Claimed, &IsStatusList](const FString& Category) -> int32
	{
		for (int32 Index = 0; Index < Lists.Num(); ++Index)
		{
			FString ListCategory;
			if (!Claimed.Contains(Index) && !IsStatusList(Lists[Index].Id) && ParseCategoryListName(Lists[Index].Name, ListCategory)
				&& ListCategory.Equals(Category, ESearchCase::IgnoreCase))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	};
	auto Keep = [&](int32 Index, const FString& DocId, const FString& Title)
	{
		Claimed.Add(Index);
		LastToDoIndex = FMath::Max(LastToDoIndex, Index);
		FPlan::FCategoryList& Kept = Plan.CategoryLists.AddDefaulted_GetRef();
		Kept.DocId = DocId;
		Kept.Category = Title;
		Kept.ListId = Lists[Index].Id;
		for (const TSharedPtr<FJsonObject>& Card : Lists[Index].Cards)
		{
			Kept.Tasks.Add(ParseCard(Card, ListNames));
		}
	};

	const int32 UncategorizedIndex = FindByName(ProtoBoyTasks::UncategorizedName);
	if (UncategorizedIndex != INDEX_NONE)
	{
		Plan.UncategorizedListId = Lists[UncategorizedIndex].Id;
		Keep(UncategorizedIndex, FString(), ProtoBoyTasks::UncategorizedName);
	}
	else
	{
		Plan.ListsToCreate.Add({ FString(), GetCategoryListName(ProtoBoyTasks::UncategorizedName) });
	}

	for (const FProtoBoyTaskCategory& Category : Categories)
	{
		const FString WantedName = GetCategoryListName(Category.Title);
		int32 Index = INDEX_NONE;
		if (const FString* Remembered = Settings->TrelloCategoryListIds.Find(Category.DocId))
		{
			Index = Lists.IndexOfByPredicate([Remembered](const ProtoBoyTrelloCategorySync::FList& L) { return L.Id == *Remembered; });
			Index = Index != INDEX_NONE && !Claimed.Contains(Index) && !IsStatusList(Lists[Index].Id) ? Index : INDEX_NONE;
		}
		if (Index == INDEX_NONE)
		{
			Index = FindByName(Category.Title);
		}
		if (Index == INDEX_NONE)
		{
			Plan.ListsToCreate.Add({ Category.DocId, WantedName });
			continue;
		}
		Plan.ListIds.Add(Category.DocId, Lists[Index].Id);
		if (!Lists[Index].Name.TrimStartAndEnd().Equals(WantedName, ESearchCase::CaseSensitive))
		{
			Plan.ListsToRename.Add({ Lists[Index].Id, Lists[Index].Name, WantedName });
		}
		Keep(Index, Category.DocId, Category.Title);
	}

	// Lists of categories that are gone: "To Do - <name>" lists nothing claimed, and remembered lists of former categories.
	TSet<FString> FormerListIds;
	for (const TPair<FString, FString>& Pair : Settings->TrelloCategoryListIds)
	{
		FormerListIds.Add(Pair.Value);
	}
	TArray<int32> RemovedIndices;
	for (int32 Index = 0; Index < Lists.Num(); ++Index)
	{
		const ProtoBoyTrelloCategorySync::FList& List = Lists[Index];
		FString Ignored;
		if (Claimed.Contains(Index) || IsStatusList(List.Id) || (!ParseCategoryListName(List.Name, Ignored) && !FormerListIds.Contains(List.Id)))
		{
			// The team's plain "To Do" list counts as where the To Do lists are, for placing new ones.
			if (!Claimed.Contains(Index) && List.Id == Settings->TrelloToDoListId)
			{
				LastToDoIndex = FMath::Max(LastToDoIndex, Index);
			}
			continue;
		}
		Plan.ListsToRemove.Add({ List.Id, List.Name, List.Cards.Num() });
		Plan.bRemapsToDoList |= List.Id == Settings->TrelloToDoListId;
		RemovedIndices.Add(Index);
	}

	// New lists go right after the last To Do list that stays (else at the left edge of the board).
	if (LastToDoIndex != INDEX_NONE)
	{
		Plan.InsertAfterPos = Lists[LastToDoIndex].Pos;
		if (Lists.IsValidIndex(LastToDoIndex + 1))
		{
			Plan.InsertBeforePos = Lists[LastToDoIndex + 1].Pos;
		}
	}
	else if (!Lists.IsEmpty())
	{
		Plan.InsertBeforePos = Lists[0].Pos;
	}

	// --- Labels: each category's by its remembered id, else by name. ---
	TSet<FString> ClaimedLabels;
	for (int32 CategoryIndex = 0; CategoryIndex < Categories.Num(); ++CategoryIndex)
	{
		const FProtoBoyTaskCategory& Category = Categories[CategoryIndex];
		const FProtoBoyTrelloItem* Label = nullptr;
		if (const FString* Remembered = Settings->TrelloCategoryLabelIds.Find(Category.DocId))
		{
			Label = Labels.FindByPredicate([Remembered, &ClaimedLabels](const FProtoBoyTrelloItem& L) { return L.Id == *Remembered && !ClaimedLabels.Contains(L.Id); });
		}
		if (!Label)
		{
			Label = Labels.FindByPredicate([&Category, &ClaimedLabels](const FProtoBoyTrelloItem& L)
			{
				return !ClaimedLabels.Contains(L.Id) && L.Name.TrimStartAndEnd().Equals(Category.Title, ESearchCase::IgnoreCase);
			});
		}
		if (!Label)
		{
			Plan.LabelsToCreate.Add({ Category.DocId, Category.Title });
			continue;
		}
		ClaimedLabels.Add(Label->Id);
		Plan.LabelIds.Add(Category.DocId, Label->Id);
		if (!Label->Name.TrimStartAndEnd().Equals(Category.Title, ESearchCase::CaseSensitive))
		{
			Plan.LabelsToRename.Add({ Label->Id, Label->Name, Category.Title });
		}
	}

	// --- Card labels: a card in a category's list has that category's label and no other category's. ---
	auto CheckCards = [&](const ProtoBoyTrelloCategorySync::FList& List, const FString& DocId, const FString& EndListName)
	{
		const FString WantedId = DocId.IsEmpty() ? FString() : Plan.LabelIds.FindRef(DocId);
		const FProtoBoyTaskCategory* Category = Categories.FindByPredicate([&DocId](const FProtoBoyTaskCategory& C) { return C.DocId == DocId; });
		for (const TSharedPtr<FJsonObject>& Card : List.Cards)
		{
			const FString CardId = ProtoBoyTrelloClient::GetString(Card, TEXT("id"));
			bool bHasWanted = false;
			const TArray<TSharedPtr<FJsonValue>>* CardLabels = nullptr;
			if (Card->TryGetArrayField(TEXT("labels"), CardLabels))
			{
				for (const TSharedPtr<FJsonValue>& LabelValue : *CardLabels)
				{
					const TSharedPtr<FJsonObject>* Label = nullptr;
					if (!LabelValue->TryGetObject(Label))
					{
						continue;
					}
					const FString LabelId = ProtoBoyTrelloClient::GetString(*Label, TEXT("id"));
					if (!WantedId.IsEmpty() && LabelId == WantedId)
					{
						bHasWanted = true;
					}
					else if (ClaimedLabels.Contains(LabelId))
					{
						// Another category's label: the card is no longer in that category.
						const FString* OtherDoc = Plan.LabelIds.FindKey(LabelId);
						Plan.LabelChanges.Add({ CardId, OtherDoc ? *OtherDoc : FString(), ProtoBoyTrelloClient::GetString(*Label, TEXT("name")), LabelId, /*bAdd*/ false, EndListName });
					}
				}
			}
			if (Category && !bHasWanted)
			{
				Plan.LabelChanges.Add({ CardId, DocId, Category->Title, WantedId, /*bAdd*/ true, EndListName });
			}
		}
	};
	const FString UncategorizedName = GetCategoryListName(ProtoBoyTasks::UncategorizedName);
	for (const FPlan::FCategoryList& Kept : Plan.CategoryLists)
	{
		const int32 Index = Lists.IndexOfByPredicate([&Kept](const ProtoBoyTrelloCategorySync::FList& L) { return L.Id == Kept.ListId; });
		CheckCards(Lists[Index], Kept.DocId, Kept.DocId.IsEmpty() ? UncategorizedName : GetCategoryListName(Kept.Category));
	}
	for (const int32 Index : RemovedIndices)
	{
		// Their cards end up in Uncategorized.
		CheckCards(Lists[Index], FString(), UncategorizedName);
	}
	return Plan;
}

void FProtoBoyTrelloClient::ApplyCategorySync(const FProtoBoyTrelloCategorySync& Plan, FDoneCallback OnDone)
{
	using FResult = TProtoBoyTaskResult<bool>;
	FText WhyNot;
	if (!IsReady(WhyNot))
	{
		OnDone(FResult::Failure(WhyNot));
		return;
	}
	const FString BoardId = GetDefault<UProtoBoySettings>()->TrelloBoardId;
	if (!IsSafeId(BoardId))
	{
		OnDone(FResult::Failure(LOCTEXT("SyncBadBoard", "Invalid board id.")));
		return;
	}
	for (const FProtoBoyTrelloCategorySync::FExtraList& List : Plan.ListsToRemove)
	{
		if (!IsSafeId(List.Id))
		{
			OnDone(FResult::Failure(LOCTEXT("SyncBadListId", "Invalid list id.")));
			return;
		}
	}
	for (const FProtoBoyTrelloCategorySync::FRename& Rename : Plan.ListsToRename)
	{
		if (!IsSafeId(Rename.Id))
		{
			OnDone(FResult::Failure(LOCTEXT("SyncBadListId", "Invalid list id.")));
			return;
		}
	}
	for (const FProtoBoyTrelloCategorySync::FRename& Rename : Plan.LabelsToRename)
	{
		if (!IsSafeId(Rename.Id))
		{
			OnDone(FResult::Failure(LOCTEXT("SyncBadLabelId", "Invalid label id.")));
			return;
		}
	}
	for (const FProtoBoyTrelloCategorySync::FLabelChange& Change : Plan.LabelChanges)
	{
		if (!IsSafeId(Change.CardId) || (!Change.LabelId.IsEmpty() && !IsSafeId(Change.LabelId)))
		{
			OnDone(FResult::Failure(LOCTEXT("SyncBadCardId", "Invalid card or label id.")));
			return;
		}
	}

	// One request at a time, in order, sharing this state (same pattern as ApplyBoardTemplate).
	using FStepDone = TFunction<void(bool, const FText&)>;
	using FStep = TFunction<void(FStepDone)>;
	struct FState
	{
		TArray<FStep> Steps;
		int32 Next = 0;
		FString UncategorizedListId;
		/** Category doc id -> list / label id, the existing ones plus the ones this sync creates. */
		TMap<FString, FString> ListIds;
		TMap<FString, FString> LabelIds;
		FDoneCallback OnDone;
		TFunction<void()> RunNext;
	};
	const TSharedRef<FState> State = MakeShared<FState>();
	State->OnDone = MoveTemp(OnDone);
	State->UncategorizedListId = Plan.UncategorizedListId;
	State->ListIds = Plan.ListIds;
	State->LabelIds = Plan.LabelIds;
	const TWeakPtr<FState> WeakState = State;
	auto Finish = [](FStepDone StepDone) { return [StepDone = MoveTemp(StepDone)](bool bOk, const FText& Error, const TSharedPtr<FJsonValue>&) { StepDone(bOk, Error); }; };

	// Rename lists and labels after their docs.
	for (const FProtoBoyTrelloCategorySync::FRename& Rename : Plan.ListsToRename)
	{
		State->Steps.Add([this, Rename, Finish](FStepDone StepDone)
		{
			TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
			Body->SetStringField(TEXT("name"), Rename.To);
			Call(TEXT("PUT"), FString::Printf(TEXT("lists/%s"), *Rename.Id), Body, Finish(MoveTemp(StepDone)));
		});
	}
	for (const FProtoBoyTrelloCategorySync::FRename& Rename : Plan.LabelsToRename)
	{
		State->Steps.Add([this, Rename, Finish](FStepDone StepDone)
		{
			TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
			Body->SetStringField(TEXT("name"), Rename.To);
			Call(TEXT("PUT"), FString::Printf(TEXT("labels/%s"), *Rename.Id), Body, Finish(MoveTemp(StepDone)));
		});
	}

	// Create the missing labels, colored in turn from Trello's palette.
	for (int32 Index = 0; Index < Plan.LabelsToCreate.Num(); ++Index)
	{
		const FProtoBoyTrelloCategorySync::FNew New = Plan.LabelsToCreate[Index];
		TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
		Body->SetStringField(TEXT("name"), New.Name);
		Body->SetStringField(TEXT("color"), ProtoBoyTrelloCategorySync::LabelColors[Index % UE_ARRAY_COUNT(ProtoBoyTrelloCategorySync::LabelColors)]);
		Body->SetStringField(TEXT("idBoard"), BoardId);
		State->Steps.Add([this, Body, New, WeakState](FStepDone StepDone)
		{
			Call(TEXT("POST"), TEXT("labels"), Body, [New, WeakState, StepDone = MoveTemp(StepDone)](bool bOk, const FText& Error, const TSharedPtr<FJsonValue>& Json)
			{
				const TSharedPtr<FJsonObject>* Label = nullptr;
				if (!bOk || !Json->TryGetObject(Label))
				{
					StepDone(false, bOk ? LOCTEXT("SyncBadNewLabel", "Trello sent an unexpected label.") : Error);
					return;
				}
				if (const TSharedPtr<FState> Pinned = WeakState.Pin())
				{
					Pinned->LabelIds.Add(New.DocId, ProtoBoyTrelloClient::GetString(*Label, TEXT("id")));
				}
				StepDone(true, FText::GetEmpty());
			});
		});
	}

	// Create the missing lists, spaced evenly between the insert positions so they keep their order.
	const int32 NumNew = Plan.ListsToCreate.Num();
	for (int32 Index = 0; Index < NumNew; ++Index)
	{
		const FProtoBoyTrelloCategorySync::FNew New = Plan.ListsToCreate[Index];
		TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
		Body->SetStringField(TEXT("name"), New.Name);
		Body->SetStringField(TEXT("idBoard"), BoardId);
		const double Fraction = double(Index + 1) / double(NumNew + 1);
		if (Plan.InsertAfterPos.IsSet() && Plan.InsertBeforePos.IsSet())
		{
			Body->SetNumberField(TEXT("pos"), Plan.InsertAfterPos.GetValue() + (Plan.InsertBeforePos.GetValue() - Plan.InsertAfterPos.GetValue()) * Fraction);
		}
		else if (Plan.InsertAfterPos.IsSet())
		{
			Body->SetNumberField(TEXT("pos"), Plan.InsertAfterPos.GetValue() + 16384.0 * (Index + 1));
		}
		else if (Plan.InsertBeforePos.IsSet())
		{
			Body->SetNumberField(TEXT("pos"), Plan.InsertBeforePos.GetValue() * Fraction);
		}
		else
		{
			Body->SetStringField(TEXT("pos"), TEXT("bottom"));
		}

		State->Steps.Add([this, Body, New, WeakState](FStepDone StepDone)
		{
			Call(TEXT("POST"), TEXT("lists"), Body, [New, WeakState, StepDone = MoveTemp(StepDone)](bool bOk, const FText& Error, const TSharedPtr<FJsonValue>& Json)
			{
				const TSharedPtr<FJsonObject>* List = nullptr;
				if (!bOk || !Json->TryGetObject(List))
				{
					StepDone(false, bOk ? LOCTEXT("SyncBadNewList", "Trello sent an unexpected list.") : Error);
					return;
				}
				if (const TSharedPtr<FState> Pinned = WeakState.Pin())
				{
					const FString Id = ProtoBoyTrelloClient::GetString(*List, TEXT("id"));
					if (New.DocId.IsEmpty())
					{
						Pinned->UncategorizedListId = Id;
					}
					else
					{
						Pinned->ListIds.Add(New.DocId, Id);
					}
				}
				StepDone(true, FText::GetEmpty());
			});
		});
	}

	// Move each extra list's cards to Uncategorized, then archive it.
	for (const FProtoBoyTrelloCategorySync::FExtraList& List : Plan.ListsToRemove)
	{
		const FString ListId = List.Id;
		if (List.NumCards > 0)
		{
			State->Steps.Add([this, ListId, BoardId, WeakState, Finish](FStepDone StepDone)
			{
				const TSharedPtr<FState> Pinned = WeakState.Pin();
				if (!Pinned.IsValid() || !IsSafeId(Pinned->UncategorizedListId))
				{
					StepDone(false, LOCTEXT("SyncNoUncategorized", "The To Do - Uncategorized list is missing, so the cards had nowhere to go."));
					return;
				}
				Call(TEXT("POST"), FString::Printf(TEXT("lists/%s/moveAllCards?idBoard=%s&idList=%s"), *ListId, *BoardId, *Pinned->UncategorizedListId), nullptr, Finish(MoveTemp(StepDone)));
			});
		}
		State->Steps.Add([this, ListId, Finish](FStepDone StepDone)
		{
			Call(TEXT("PUT"), FString::Printf(TEXT("lists/%s/closed?value=true"), *ListId), nullptr, Finish(MoveTemp(StepDone)));
		});
	}

	// Add and remove the cards' category labels (after the moves, so cards from archived lists are already in Uncategorized).
	for (const FProtoBoyTrelloCategorySync::FLabelChange& Change : Plan.LabelChanges)
	{
		State->Steps.Add([this, Change, WeakState, Finish](FStepDone StepDone)
		{
			FString LabelId = Change.LabelId;
			if (LabelId.IsEmpty())
			{
				const TSharedPtr<FState> Pinned = WeakState.Pin();
				LabelId = Pinned.IsValid() ? Pinned->LabelIds.FindRef(Change.DocId) : FString();
			}
			if (!IsSafeId(LabelId))
			{
				StepDone(false, FText::Format(LOCTEXT("SyncNoLabel", "The \"{0}\" label is missing."), FText::FromString(Change.LabelName)));
				return;
			}
			if (Change.bAdd)
			{
				Call(TEXT("POST"), FString::Printf(TEXT("cards/%s/idLabels?value=%s"), *Change.CardId, *LabelId), nullptr, Finish(MoveTemp(StepDone)));
			}
			else
			{
				Call(TEXT("DELETE"), FString::Printf(TEXT("cards/%s/idLabels/%s"), *Change.CardId, *LabelId), nullptr, Finish(MoveTemp(StepDone)));
			}
		});
	}

	// Holds State weakly (State owns this function); each step's callback holds it strongly while a request is in flight.
	const bool bRemapsToDoList = Plan.bRemapsToDoList;
	State->RunNext = [WeakState, bRemapsToDoList]()
	{
		const TSharedPtr<FState> Pinned = WeakState.Pin();
		if (!Pinned.IsValid())
		{
			return;
		}
		if (!Pinned->Steps.IsValidIndex(Pinned->Next))
		{
			RememberCategoryIds(Pinned->ListIds, Pinned->LabelIds);
			if (bRemapsToDoList && !Pinned->UncategorizedListId.IsEmpty())
			{
				UProtoBoySettings* Settings = GetMutableDefault<UProtoBoySettings>();
				Settings->TrelloToDoListId = Pinned->UncategorizedListId;
				Settings->TryUpdateDefaultConfigFile();
			}
			FDoneCallback Callback = MoveTemp(Pinned->OnDone);
			Callback(FResult::Success(true));
			return;
		}
		const TSharedRef<FState> Strong = Pinned.ToSharedRef();
		Pinned->Steps[Pinned->Next++]([Strong](bool bOk, const FText& Error)
		{
			if (!bOk)
			{
				// Keep what was made, so the next sync doesn't make it again.
				RememberCategoryIds(Strong->ListIds, Strong->LabelIds);
				FDoneCallback Callback = MoveTemp(Strong->OnDone);
				Callback(FResult::Failure(Error));
				return;
			}
			Strong->RunNext();
		});
	};
	State->RunNext();
}

#undef LOCTEXT_NAMESPACE
