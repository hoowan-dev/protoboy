#pragma once

#include "MCP/PBMCPConnection.h"
#include "Widgets/SCompoundWidget.h"

class FProtoBoyTaskGenerationRun;
class SMultiLineEditableTextBox;
enum class ECheckBoxState : uint8;
class SScrollBox;
class SWidgetSwitcher;
class SWindow;

/**
 * "Generate Tasks" window, opened from the Tasks page. The first step explains what it does (and that the agent's
 * generate-tasks prompt does the same from the agent itself), then picks the agent, the task categories (at least one)
 * and optional extra context. Generate goes to the second step, which shows the agent's progress
 * (FProtoBoyTaskGenerationRun), like Set Up Docs from Template > Generate from the Project. Next goes to the third step:
 * the ideas as nested checkboxes (category, then its ideas), all ticked; Done adds the ticked ones to the board.
 * Cancel, and the window's close button, ask first while the agent works, while adding, and before discarding ideas.
 */
class SProtoBoyGenerateTasksDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProtoBoyGenerateTasksDialog) {}
		SLATE_ARGUMENT(TWeakPtr<SWindow>, ParentWindow)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SProtoBoyGenerateTasksDialog() override;

	/** Opens the window as a modal over ParentWidget's window. Adding tasks checks the board again (FProtoBoyTaskBoard). */
	static void Open(const TSharedPtr<SWidget>& ParentWidget);

private:
	enum class EStep : int32
	{
		Setup,
		Generating,
		Review,
	};

	/** Description, agent, categories and extra context, with Generate. */
	TSharedRef<SWidget> MakeSetupStep();
	/** The agent's progress log, with Cancel (Close once it's done) and Next. */
	TSharedRef<SWidget> MakeGeneratingStep();
	bool CanGenerate() const;
	FReply OnGenerateClicked();
	/** Polls the run and adds its new log lines. */
	void PollGeneration();
	/** The ideas as nested checkboxes, with Done. */
	TSharedRef<SWidget> MakeReviewStep();
	/** Fill the review list from the run's ideas and show the step. */
	void ShowReview();
	/** One category's checkbox (ticks or clears all of its ideas) and its ideas below it. */
	TSharedRef<SWidget> MakeReviewCategory(const FText& Title, const TArray<int32>& Indices);
	ECheckBoxState GetCategoryState(const TArray<int32>& Indices) const;
	FReply OnDoneClicked();
	/** Ideas are waiting to be kept or discarded (none added yet). */
	bool HasPendingIdeas() const;
	/** Asks before throwing away the ideas. */
	bool ConfirmDiscard() const;
	bool IsGenerating() const;
	/** Asks before cancelling while the agent is working. */
	bool ConfirmCancelGeneration() const;
	/** The window's close button: the same as the step's Cancel. */
	void OnWindowCloseRequested(const TSharedRef<SWindow>& Window);
	void CloseWindow();

	TWeakPtr<SWindow> ParentWindow;
	TSharedPtr<SWidgetSwitcher> StepSwitcher;
	EStep CurrentStep = EStep::Setup;

	TOptional<EProtoBoyMCPClient> SelectedAgent;
	/** Task category doc ids, in map order, and the ones ticked. */
	TArray<FString> CategoryIds;
	TSet<FString> SelectedCategories;
	TSharedPtr<SMultiLineEditableTextBox> ContextBox;
	FText GenerateError;

	TSharedPtr<FProtoBoyTaskGenerationRun> Run;
	TSharedPtr<SScrollBox> LogScroll;
	int32 LogShown = 0;

	TSharedPtr<class SBox> ReviewBox;
	/** Indices into the run's ideas that are ticked. */
	TSet<int32> SelectedIdeas;
	/** How many Done is adding, and why some couldn't be added. */
	int32 NumToAdd = 0;
	TArray<FText> AddErrors;
	/** Done ran to the end (some may have failed). */
	bool bAddFinished = false;
	/** Set once closing has been confirmed, so it doesn't ask again. */
	bool bClosing = false;
};
