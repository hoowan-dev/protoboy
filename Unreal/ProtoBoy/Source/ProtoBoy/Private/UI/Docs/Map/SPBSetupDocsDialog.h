#pragma once

#include "MCP/PBMCPConnection.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Views/SListView.h"

struct FProtoBoyDocTemplate;
class FProtoBoyDocGenerationRun;
class SBox;
class SScrollBox;
class SWindow;
class SWidgetSwitcher;

/**
 * "Set Up Docs from Template" window, opened from the doc map (and the Obsidian page of Connect to Documentation).
 * The first step picks how to start: from a template, or generated from what's already in the project. The project
 * path explains what happens, lists the user's agents, warns that it overwrites the docs, then shows the agent's
 * progress (FProtoBoyDocGenerationRun) until Finish. Cancel, and the window's close button, ask first and clear the docs. The template path lists the doc-set templates (by genre,
 * then by size) with a preview of the selected one: its design pillars and the docs it creates. Its last step shows
 * exactly what applying it writes and what it leaves alone, then applies it.
 * Applying replaces the documentation: the docs on the map go to the trash first (FProtoBoyDocMap::ApplyTemplate).
 */
class SProtoBoySetupDocsDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProtoBoySetupDocsDialog) {}
		SLATE_ARGUMENT(TWeakPtr<SWindow>, ParentWindow)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SProtoBoySetupDocsDialog() override;

	/** Opens the window as a modal over ParentWidget's window. */
	static void Open(const TSharedPtr<SWidget>& ParentWidget);

private:
	/** The window's steps, in StepSwitcher slot order. */
	enum class EStep : int32
	{
		Start,
		Choose,
		Confirm,
		FromProject,
		Generating,
	};

	void ShowStep(EStep Step);

	/** How to start: from a template (Choose) or from the project's contents (FromProject). */
	TSharedRef<SWidget> MakeStartStep();
	/** One large option on the start step: icon, title and description; clicking it goes to Step. */
	TSharedRef<SWidget> MakeStartOption(const FName& Icon, const FText& Title, const FText& Description, EStep Step);
	/** What generating from the project does, the user's agents to pick from, and Generate. */
	TSharedRef<SWidget> MakeFromProjectStep();
	/** Warns that the docs are overwritten, then starts the agent and shows its progress. */
	FReply OnGenerateClicked();
	/** The agent's progress log, with Cancel and Finish. */
	TSharedRef<SWidget> MakeGeneratingStep();
	/** Polls the run and adds its new log lines. Stops once it's finished. */
	void PollGeneration();
	/** Asks before cancelling a generation (the docs are cleared). */
	bool ConfirmCancelGeneration() const;
	/** The window's close button: while generating it's the same as Cancel. */
	void OnWindowCloseRequested(const TSharedRef<SWindow>& Window);
	void CloseWindow();

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
	/** Docs the selected template would move to the trash first. */
	int32 PlannedRemoved = 0;
	/** The confirm step's choice: fill the new docs' sections with the template's example content, or leave them blank. */
	bool bWriteExamples = false;

	EStep CurrentStep = EStep::Start;
	TOptional<EProtoBoyMCPClient> SelectedAgent;
	TUniquePtr<FProtoBoyDocGenerationRun> Run;
	TSharedPtr<SScrollBox> LogScroll;
	/** Log lines already added to LogScroll. */
	int32 LogShown = 0;
	FText GenerateError;
	/** Set by Finish and Cancel, so closing doesn't ask again. */
	bool bClosing = false;
};
