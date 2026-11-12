#pragma once

#include "Widgets/SCompoundWidget.h"

class SWindow;
class SWidgetSwitcher;

/**
 * "Setup Pillars from Template" window, opened from Architecture > Pillars. Starts on a choice between two ways to
 * build the pillar tree, each of which moves the window on to its own step:
 * - Build From Template: pick a preset by game genre and scope.
 * - Build From Existing Project: an LLM reads the work done so far and proposes pillars around it.
 * Both steps are stubs for now.
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
	};

	void ShowStep(EStep Step);

	/** Choose: the two option buttons. */
	TSharedRef<SWidget> MakeChooseStep();
	/** One large option button: title, description, and a click that moves to Target. */
	TSharedRef<SWidget> MakeOption(const FText& Title, const FText& Description, FName Icon, EStep Target);

	/** Build From Template: pick a genre and scope preset. */
	TSharedRef<SWidget> MakeFromTemplateStep();
	/** Build From Existing Project: have an LLM propose pillars from the project so far. */
	TSharedRef<SWidget> MakeFromExistingProjectStep();

	/** Shared layout for a step after Choose: title, body, and a Back button to Choose. */
	TSharedRef<SWidget> MakeSubStep(const FText& Title, const TSharedRef<SWidget>& Body);

	TWeakPtr<SWindow> ParentWindow;
	TSharedPtr<SWidgetSwitcher> StepSwitcher;
};
