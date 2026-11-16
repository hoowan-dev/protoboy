#pragma once

#include "CoreMinimal.h"
#include "Tasks/PBTaskProvider.h"

/**
 * What it takes to make the board match the doc map's task categories: one "To Do - Uncategorized" list, plus one
 * "To Do - <Category>" list and one label named after each category. Other lists and labels are left alone.
 *
 * A category's list and label are tied to its doc's id (UProtoBoySettings::TrelloCategoryListIds / LabelIds), so
 * renaming the doc renames them and its cards stay put. Lists and labels without a remembered id are matched by name
 * (and remembered from then on).
 */
struct FProtoBoyTrelloCategorySync
{
	/** A list or label to create. DocId is the category's doc (empty for Uncategorized's list). */
	struct FNew
	{
		FString DocId;
		FString Name;
	};

	/** A list or label whose name no longer matches its category (the doc was renamed). */
	struct FRename
	{
		FString Id;
		FString From;
		FString To;
	};

	/** A "To Do - <Category>" list whose category no longer exists. */
	struct FExtraList
	{
		FString Id;
		FString Name;
		int32 NumCards = 0;
	};

	/** A category label to add to a card, or another category's label to take off it. */
	struct FLabelChange
	{
		FString CardId;
		/** The category the label belongs to (adding) or is named after (removing). */
		FString DocId;
		FString LabelName;
		/** Empty when the label is in LabelsToCreate (resolved when it's created). */
		FString LabelId;
		bool bAdd = true;
		/** The list the card ends up in, for the window ("To Do - Combat"). */
		FString ListName;
	};

	/** A category's To Do list that stays (or Uncategorized's, with an empty DocId), with its open cards in board order. */
	struct FCategoryList
	{
		FString DocId;
		/** The category's title on the doc map (Uncategorized for that list). */
		FString Category;
		FString ListId;
		TArray<FProtoBoyTask> Tasks;
	};

	/** The lists that stay, with their tasks: what the Tasks page shows under each category. */
	TArray<FCategoryList> CategoryLists;
	/** Open cards in the team's In Progress list (TrelloInProgressListId), in board order. */
	TArray<FProtoBoyTask> InProgressTasks;
	/** Open cards in the team's Done list (TrelloDoneListId), most recently active first. */
	TArray<FProtoBoyTask> DoneTasks;

	/** Lists to create, in board order ("To Do - Uncategorized" first when it's missing). */
	TArray<FNew> ListsToCreate;
	/** Category lists to rename after their docs. */
	TArray<FRename> ListsToRename;
	/** Lists whose cards move to "To Do - Uncategorized" before the list is archived (Trello can't delete lists). */
	TArray<FExtraList> ListsToRemove;
	/** Category labels to create. Uncategorized gets none. */
	TArray<FNew> LabelsToCreate;
	/** Category labels to rename after their docs. */
	TArray<FRename> LabelsToRename;
	/**
	 * Every card in a category's list carries that category's label and no other category's, so a card can be traced
	 * to its category after it moves on (In Progress, Done). Cards in Uncategorized (and those moved there from
	 * ListsToRemove) carry no category label.
	 */
	TArray<FLabelChange> LabelChanges;

	/** Category doc id -> its list / label on the board, for the ones that exist. Remembered in the team settings. */
	TMap<FString, FString> ListIds;
	TMap<FString, FString> LabelIds;
	/** The existing "To Do - Uncategorized" list; empty when it has to be created. */
	FString UncategorizedListId;
	/** True when the team's mapped To Do list (TrelloToDoListId) is one of ListsToRemove: it moves to Uncategorized. */
	bool bRemapsToDoList = false;
	/** New lists go between these positions (board order), after the existing To Do lists. Unset = board edge. */
	TOptional<double> InsertAfterPos;
	TOptional<double> InsertBeforePos;

	bool IsInSync() const
	{
		return ListsToCreate.IsEmpty() && ListsToRename.IsEmpty() && ListsToRemove.IsEmpty()
			&& LabelsToCreate.IsEmpty() && LabelsToRename.IsEmpty() && LabelChanges.IsEmpty();
	}
};
