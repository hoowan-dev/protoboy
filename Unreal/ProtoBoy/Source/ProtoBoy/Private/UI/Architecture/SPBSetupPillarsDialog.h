#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Views/SListView.h"

struct FProtoBoyPillarTemplate;
class SProtoBoyPillarTreePreview;
class SWindow;
class SWidgetSwitcher;

/**
 * "Setup Pillars from Template" window, opened from Architecture > Pillars. Starts on a choice between two ways to
 * build the pillar tree, each of which moves the window on to its own step:
 * - Build From Template: a list of presets (by game genre) and a preview of the selected one's tree. Next
 *   moves to a confirmation step that shows the template again and says what it overwrites, then applies it.
 * - Build From Existing Project: an LLM reads the work done so far and proposes pillars around it (blank for now).
 */
class SProtoBoySetupPillarsDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProtoBoySetupPillarsDialog) {}
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
		FromTemplate,
		FromExistingProject,
		/** After FromTemplate: the selected template, and confirmation that it replaces the current pillars. */
		ConfirmTemplate,
	};

	void ShowStep(EStep Step);

	/** Choose: the two option buttons. */
	TSharedRef<SWidget> MakeChooseStep();
	/** One large option button: title, description, and a click that moves to Target. */
	TSharedRef<SWidget> MakeOption(const FText& Title, const FText& Description, FName Icon, EStep Target);

	/** Build From Template: the template list, and the selected template's description and tree preview. */
	TSharedRef<SWidget> MakeFromTemplateStep();
	TSharedRef<ITableRow> OnGenerateTemplateRow(TSharedPtr<FProtoBoyPillarTemplate> Template, const TSharedRef<STableViewBase>& OwnerTable);
	void OnTemplateSelected(TSharedPtr<FProtoBoyPillarTemplate> Template, ESelectInfo::Type SelectInfo);
	/** Confirm Template: the selected template's tree, and what applying it overwrites. */
	TSharedRef<SWidget> MakeConfirmTemplateStep();
	/** Replaces the pillar tree with the selected template's, then closes the window. */
	FReply OnApplyTemplateClicked();

	/** Build From Existing Project: have an LLM propose pillars from the project so far. */
	TSharedRef<SWidget> MakeFromExistingProjectStep();

	/** Shared layout for a step after Choose: title, body, a Back button to BackTo, and the step's Action on the right. */
	TSharedRef<SWidget> MakeSubStep(const FText& Title, const TSharedRef<SWidget>& Body, EStep BackTo,
		const TSharedRef<SWidget>& Action = SNullWidget::NullWidget);

	TWeakPtr<SWindow> ParentWindow;
	TSharedPtr<SWidgetSwitcher> StepSwitcher;

	TArray<TSharedPtr<FProtoBoyPillarTemplate>> Templates;
	TSharedPtr<SListView<TSharedPtr<FProtoBoyPillarTemplate>>> TemplateList;
	TSharedPtr<FProtoBoyPillarTemplate> SelectedTemplate;
	TSharedPtr<SProtoBoyPillarTreePreview> TemplatePreview;
	TSharedPtr<SProtoBoyPillarTreePreview> ConfirmPreview;
};
