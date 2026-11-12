#pragma once

#include "Widgets/Input/SComboBox.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

class FProtoBoyPillarService;
class SBox;

/**
 * Planning > Pillars: author the pillar tree (add, nest, reorder, rename, remove) and edit the selected pillar's
 * details, then generate docs from it (with a preview). Edits save to the pillar file immediately.
 */
class SProtoBoyPillarTreeEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProtoBoyPillarTreeEditor) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SProtoBoyPillarTreeEditor() override;

private:
	/** A tree row: just the pillar's id (data lives in FProtoBoyPillarService). */
	struct FNode
	{
		FString Id;
		TArray<TSharedPtr<FNode>> Children;
	};
	using FNodePtr = TSharedPtr<FNode>;

	TSharedRef<SWidget> MakeToolbar();
	TSharedRef<ITableRow> OnGenerateRow(FNodePtr Node, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetChildren(FNodePtr Node, TArray<FNodePtr>& OutChildren);
	void OnSelectionChanged(FNodePtr Node, ESelectInfo::Type SelectInfo);

	/** Rebuild rows from the service, keeping the selection. Bound to the service's OnChanged. */
	void RebuildTree();
	/** Rebuild the details pane for the selected pillar. */
	void RebuildDetails();
	/**
	 * Rebuild the details pane on the next frame. Edits made in the pane trigger a rebuild, and rebuilding immediately
	 * would destroy the text box or combo box that is still inside its own commit callback.
	 */
	void ScheduleDetailsRebuild();
	bool bDetailsRebuildPending = false;
	TSharedRef<SWidget> MakeDetails(const FString& Id);
	TSharedRef<SWidget> MakeLabel(const FText& Text);

	void Select(const FString& Id);
	FReply OnAddClicked(bool bAsChild);
	FReply OnMoveClicked(int32 Delta);
	FReply OnRemoveClicked();
	FReply OnStarterClicked();
	FReply OnGenerateDocsClicked();

	static FProtoBoyPillarService* GetService();

	TArray<FNodePtr> Roots;
	TSharedPtr<STreeView<FNodePtr>> TreeView;
	TSharedPtr<SBox> DetailsBox;
	FString SelectedId;

	/** Options for the details pane's parent picker ("" = top level). */
	TArray<TSharedPtr<FString>> ParentOptions;

	FText EditError;
	FDelegateHandle ChangedHandle;
	/** Set while we rebuild, so our own selection changes don't rebuild again. */
	bool bRebuilding = false;
};
