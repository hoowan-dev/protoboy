#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Views/SListView.h"

struct FProtoBoyDocTemplate;
class SBox;
class SWindow;
class SWidgetSwitcher;

/**
 * "Set Up Docs from Template" window, opened from the doc map (and the Obsidian page of Connect to Documentation).
 * Step one lists the doc-set templates (by genre, then by size) with a preview of the selected one: its design pillars
 * and the docs it creates. Step two shows exactly what applying it writes and what it leaves alone, then applies it.
 * Applying never overwrites a doc (FProtoBoyDocMap::ApplyTemplate).
 */
class SProtoBoySetupDocsDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProtoBoySetupDocsDialog) {}
		SLATE_ARGUMENT(TWeakPtr<SWindow>, ParentWindow)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Opens the window as a modal over ParentWidget's window. */
	static void Open(const TSharedPtr<SWidget>& ParentWidget);

private:
	/** The window's steps, in StepSwitcher slot order. */
	enum class EStep : int32
	{
		Choose,
		Confirm,
	};

	void ShowStep(EStep Step);

	TSharedRef<SWidget> MakeChooseStep();
	TSharedRef<ITableRow> OnGenerateTemplateRow(TSharedPtr<FProtoBoyDocTemplate> Template, const TSharedRef<STableViewBase>& OwnerTable);
	void OnTemplateSelected(TSharedPtr<FProtoBoyDocTemplate> Template, ESelectInfo::Type SelectInfo);
	/** The selected template's description, design pillars and doc tree. */
	TSharedRef<SWidget> MakePreview(const FProtoBoyDocTemplate& Template);

	TSharedRef<SWidget> MakeConfirmStep();
	/** What applying the selected template writes and keeps, from FProtoBoyDocMap::PlanTemplate. */
	TSharedRef<SWidget> MakePlan(const FProtoBoyDocTemplate& Template);
	FReply OnApplyClicked();

	/** Title, body, Back (or Cancel) on the left and the step's action on the right. */
	TSharedRef<SWidget> MakeStepLayout(const FText& Title, const TSharedRef<SWidget>& Body, TOptional<EStep> BackTo, const TSharedRef<SWidget>& Action);

	TWeakPtr<SWindow> ParentWindow;
	TSharedPtr<SWidgetSwitcher> StepSwitcher;

	TArray<TSharedPtr<FProtoBoyDocTemplate>> Templates;
	TSharedPtr<SListView<TSharedPtr<FProtoBoyDocTemplate>>> TemplateList;
	TSharedPtr<FProtoBoyDocTemplate> SelectedTemplate;
	TSharedPtr<SBox> PreviewBox;
	TSharedPtr<SBox> PlanBox;
	/** Set when applying fails; shown on the confirm step. */
	FText ApplyError;
	int32 PlannedCount = 0;
};
