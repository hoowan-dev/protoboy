#include "UI/Tasks/SPBGenerateTasksDialog.h"

#include "Docs/PBDocMap.h"
#include "Framework/Application/SlateApplication.h"
#include "MCP/PBAgentRun.h"
#include "MCP/PBMCPClientConfig.h"
#include "Misc/MessageDialog.h"
#include "PBSubsystem.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "Tasks/Generation/PBTaskGeneration.h"
#include "Tasks/PBTaskProvider.h"
#include "UI/PBStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ProtoBoyGenerateTasksDialog"

namespace ProtoBoyGenerateTasksDialog
{
	TSharedRef<SWidget> MakeHeading(const FText& Text)
	{
		return SNew(STextBlock)
			.Text(Text)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10));
	}

	TSharedRef<SWidget> MakeParagraph(const FText& Text)
	{
		return SNew(STextBlock).Text(Text).AutoWrapText(true);
	}
}

void SProtoBoyGenerateTasksDialog::Open(const TSharedPtr<SWidget>& ParentWidget, const FExistingTasks& ExistingTasks, TFunction<void(bool bAddedTasks)> OnClosed)
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Generate Tasks"))
		.ClientSize(FVector2D(720.f, 640.f))
		.SupportsMinimize(false)
		.SupportsMaximize(false);

	const TSharedRef<SProtoBoyGenerateTasksDialog> Dialog = SNew(SProtoBoyGenerateTasksDialog)
		.ParentWindow(Window)
		.ExistingTasks(ExistingTasks);
	Window->SetContent(Dialog);
	Window->SetRequestDestroyWindowOverride(FRequestDestroyWindowOverride::CreateSP(Dialog, &SProtoBoyGenerateTasksDialog::OnWindowCloseRequested));
	FSlateApplication::Get().AddModalWindow(Window, ParentWidget);

	// Modal: this runs once the window has closed.
	if (OnClosed)
	{
		OnClosed(Dialog->Run.IsValid() && Dialog->Run->GetNumAdded() > 0);
	}
}

SProtoBoyGenerateTasksDialog::~SProtoBoyGenerateTasksDialog()
{
	if (Run.IsValid())
	{
		Run->Cancel();
	}
}

void SProtoBoyGenerateTasksDialog::Construct(const FArguments& InArgs)
{
	ParentWindow = InArgs._ParentWindow;
	ExistingTasks = InArgs._ExistingTasks;

	Agents = FProtoBoyAgentRun::GetSetUpAgents();
	for (const EProtoBoyMCPClient Agent : Agents)
	{
		FText Ignored;
		if (FProtoBoyAgentRun::CanRun(Agent, Ignored))
		{
			SelectedAgent = Agent;
			break;
		}
	}
	if (const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get())
	{
		for (const FProtoBoyDocNode* Doc : Subsystem->GetDocMap().GetTaskCategories())
		{
			CategoryIds.Add(Doc->Id);
		}
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		.Padding(16.f)
		[
			SAssignNew(StepSwitcher, SWidgetSwitcher)
			.WidgetIndex_Lambda([this]() { return static_cast<int32>(CurrentStep); })
			+ SWidgetSwitcher::Slot() [ MakeSetupStep() ]       // EStep::Setup
			+ SWidgetSwitcher::Slot() [ MakeGeneratingStep() ]  // EStep::Generating
			+ SWidgetSwitcher::Slot() [ MakeReviewStep() ]      // EStep::Review
		]
	];
}

// --- Setup ----------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> SProtoBoyGenerateTasksDialog::MakeSetupStep()
{
	const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	const FProtoBoyDocMap* Map = Subsystem ? &Subsystem->GetDocMap() : nullptr;

	// Agents
	TSharedRef<SVerticalBox> AgentList = SNew(SVerticalBox);
	for (const EProtoBoyMCPClient Agent : Agents)
	{
		AgentList->AddSlot().AutoHeight().Padding(0.f, 2.f)[ MakeAgentRow(Agent) ];
	}
	if (Agents.IsEmpty())
	{
		AgentList->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoAgents", "No agents are set up yet. Connect Claude Code or Gemini CLI in MCP Settings (Setup > MCP on the dashboard), then come back."))
			.ColorAndOpacity(FStyleColors::Warning)
			.AutoWrapText(true)
		];
	}

	// Categories
	TSharedRef<SVerticalBox> CategoryList = SNew(SVerticalBox);
	for (const FString& Id : CategoryIds)
	{
		const FProtoBoyDocNode* Doc = Map ? Map->Find(Id) : nullptr;
		if (!Doc)
		{
			continue;
		}
		CategoryList->AddSlot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([this, Id]() { return SelectedCategories.Contains(Id) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([this, Id](ECheckBoxState State)
			{
				if (State == ECheckBoxState::Checked)
				{
					SelectedCategories.Add(Id);
				}
				else
				{
					SelectedCategories.Remove(Id);
				}
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f, 0.f, 8.f, 0.f)
				[
					SNew(STextBlock).Text(FText::FromString(Doc->Title))
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(Doc->Path)).ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]
		];
	}
	if (CategoryIds.IsEmpty())
	{
		CategoryList->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoCategories", "No task categories yet. Mark docs as task categories on the doc map first (right-click a doc > Task Category)."))
			.ColorAndOpacity(FStyleColors::Warning)
			.AutoWrapText(true)
		];
	}

	const TSharedRef<SWidget> Body = SNew(SScrollBox)

		+ SScrollBox::Slot()
		.Padding(0.f, 0.f, 0.f, 8.f)
		[
			ProtoBoyGenerateTasksDialog::MakeParagraph(FText::Format(LOCTEXT("What", "Generate possible tasks for your task categories with an LLM. One of your agents reads each category's doc and the docs below it on the doc map, looks at the project to see what's already built, and drafts tasks small enough for one session each. ProtoBoy adds them to each category's To Do list on the board, tagged with its label (tasks that fit no category go to \"{0}\"). They're ideas: remove the ones you don't want."),
				FText::FromString(ProtoBoyTasks::UncategorizedName)))
		]

		+ SScrollBox::Slot()
		.Padding(0.f, 0.f, 0.f, 8.f)
		[
			ProtoBoyGenerateTasksDialog::MakeParagraph(LOCTEXT("FromAgent", "You can also do this from your agent directly, and talk the tasks through before they're created: invoke ProtoBoy's generate-tasks prompt, as /mcp__protoboy__generate-tasks in Claude Code or /generate-tasks in Gemini CLI, with the category's name."))
		]

		+ SScrollBox::Slot()
		.Padding(0.f, 6.f, 0.f, 4.f)
		[
			ProtoBoyGenerateTasksDialog::MakeHeading(LOCTEXT("AgentHeading", "Agent"))
		]

		+ SScrollBox::Slot()
		[
			AgentList
		]

		+ SScrollBox::Slot()
		.Padding(0.f, 12.f, 0.f, 4.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				ProtoBoyGenerateTasksDialog::MakeHeading(LOCTEXT("CategoriesHeading", "Task categories"))
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PickOne", "Pick at least one."))
				.ColorAndOpacity_Lambda([this]() { return SelectedCategories.IsEmpty() ? FSlateColor(FStyleColors::Warning) : FSlateColor::UseSubduedForeground(); })
			]
		]

		+ SScrollBox::Slot()
		[
			CategoryList
		]

		+ SScrollBox::Slot()
		.Padding(0.f, 12.f, 0.f, 4.f)
		[
			ProtoBoyGenerateTasksDialog::MakeHeading(LOCTEXT("ContextHeading", "Extra context (optional)"))
		]

		+ SScrollBox::Slot()
		[
			SNew(SBox)
			.MinDesiredHeight(80.f)
			[
				SAssignNew(ContextBox, SMultiLineEditableTextBox)
				.HintText(LOCTEXT("ContextHint", "E.g. \"Focus on what we need for the vertical slice\" or \"Skip anything about multiplayer\"."))
				.AutoWrapText(true)
			]
		]

		+ SScrollBox::Slot()
		.Padding(0.f, 8.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Visibility_Lambda([this]() { return GenerateError.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
			.Text_Lambda([this]() { return GenerateError; })
			.ColorAndOpacity(FStyleColors::Error)
			.AutoWrapText(true)
		];

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 10.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SetupTitle", "Generate Tasks"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			Body
		]

		// Cancel on the left, Generate on the right
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 12.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("Cancel", "Cancel"))
				.OnClicked_Lambda([this]() { CloseWindow(); return FReply::Handled(); })
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNullWidget::NullWidget
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
				.Text(LOCTEXT("Generate", "Generate"))
				.ToolTipText_Lambda([this]()
				{
					return CanGenerate()
						? LOCTEXT("GenerateTip", "Start the agent. Its tasks are added to the board when it's done.")
						: LOCTEXT("GenerateNeeds", "Pick an agent and at least one task category.");
				})
				.IsEnabled_Lambda([this]() { return CanGenerate(); })
				.OnClicked(this, &SProtoBoyGenerateTasksDialog::OnGenerateClicked)
			]
		];
}

TSharedRef<SWidget> SProtoBoyGenerateTasksDialog::MakeAgentRow(EProtoBoyMCPClient Agent)
{
	FText WhyNot;
	const bool bCanRun = FProtoBoyAgentRun::CanRun(Agent, WhyNot);
	const EProtoBoyLLMProvider Provider = Agent == EProtoBoyMCPClient::GeminiCLI ? EProtoBoyLLMProvider::Gemini : EProtoBoyLLMProvider::Claude;

	return SNew(SCheckBox)
		.Style(FAppStyle::Get(), "RadioButton")
		.IsEnabled(bCanRun)
		.IsChecked_Lambda([this, Agent]() { return SelectedAgent.IsSet() && SelectedAgent.GetValue() == Agent ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
		.OnCheckStateChanged_Lambda([this, Agent](ECheckBoxState) { SelectedAgent = Agent; })
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.f, 0.f, 6.f, 0.f)
			[
				SNew(SImage)
				.Image(FProtoBoyStyle::Get().GetBrush(FProtoBoyMCPClientConfig::GetProviderIcon(Provider)))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(FProtoBoyMCPClientConfig::GetClientName(Agent))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Visibility(bCanRun ? EVisibility::Collapsed : EVisibility::Visible)
					.Text(WhyNot)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.AutoWrapText(true)
				]
			]
		];
}

bool SProtoBoyGenerateTasksDialog::CanGenerate() const
{
	FText Ignored;
	return SelectedAgent.IsSet() && FProtoBoyAgentRun::CanRun(SelectedAgent.GetValue(), Ignored) && !SelectedCategories.IsEmpty();
}

FReply SProtoBoyGenerateTasksDialog::OnGenerateClicked()
{
	if (!CanGenerate())
	{
		return FReply::Handled();
	}
	const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	const FProtoBoyDocMap* Map = Subsystem ? &Subsystem->GetDocMap() : nullptr;

	// In map order, with the tasks each already has.
	TArray<FProtoBoyTaskGenerationRun::FCategory> Categories;
	for (const FString& Id : CategoryIds)
	{
		const FProtoBoyDocNode* Doc = Map ? Map->Find(Id) : nullptr;
		if (Doc && SelectedCategories.Contains(Id))
		{
			FProtoBoyTaskGenerationRun::FCategory& Category = Categories.AddDefaulted_GetRef();
			Category.DocId = Id;
			if (const TArray<FString>* Existing = ExistingTasks.Find(Doc->Title.TrimStartAndEnd().ToLower()))
			{
				Category.ExistingTasks = *Existing;
			}
		}
	}

	Run = MakeShared<FProtoBoyTaskGenerationRun>();
	LogShown = 0;
	LogScroll->ClearChildren();
	GenerateError = FText::GetEmpty();
	if (!Run->Start(SelectedAgent.GetValue(), Categories, ContextBox.IsValid() ? ContextBox->GetText().ToString() : FString(), GenerateError))
	{
		Run.Reset();
		return FReply::Handled();
	}
	CurrentStep = EStep::Generating;
	PollGeneration();
	RegisterActiveTimer(0.1f, FWidgetActiveTimerDelegate::CreateLambda([this](double, float)
	{
		PollGeneration();
		return Run.IsValid() && !Run->IsFinished() ? EActiveTimerReturnType::Continue : EActiveTimerReturnType::Stop;
	}));
	return FReply::Handled();
}

// --- Generating -----------------------------------------------------------------------------------------------------

TSharedRef<SWidget> SProtoBoyGenerateTasksDialog::MakeGeneratingStep()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("GeneratingTitle", "Generating Tasks"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
		]

		// Status: what's happening, with a throbber while the agent runs
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 8.f, 0.f, 6.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(SCircularThrobber)
				.Radius(8.f)
				.Visibility_Lambda([this]() { return IsGenerating() ? EVisibility::Visible : EVisibility::Collapsed; })
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text_Lambda([this]()
				{
					if (!Run.IsValid() || Run->IsRunning())
					{
						return FText::Format(LOCTEXT("StatusRunning", "{0} is reading the docs and drafting tasks..."),
							SelectedAgent.IsSet() ? FProtoBoyMCPClientConfig::GetClientName(SelectedAgent.GetValue()) : FText::GetEmpty());
					}
					return Run->Succeeded()
						? FText::Format(LOCTEXT("StatusDone", "Done. {0} task {0}|plural(one=idea is,other=ideas are) ready."), FText::AsNumber(Run->GetIdeas().Num()))
						: LOCTEXT("StatusFailed", "The agent didn't finish, so there are no task ideas. See the log for why.");
				})
				.ColorAndOpacity_Lambda([this]()
				{
					return Run.IsValid() && Run->IsFinished() && !Run->Succeeded() ? FSlateColor(FStyleColors::Error) : FSlateColor::UseForeground();
				})
			]
		]

		// While the agent runs: closing early cancels (red). Once it's done: complete (green).
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 8.f)
		[
			SNew(STextBlock)
			.Visibility_Lambda([this]()
			{
				return Run.IsValid() && (Run->IsRunning() || Run->Succeeded()) ? EVisibility::Visible : EVisibility::Collapsed;
			})
			.Text_Lambda([this]()
			{
				return Run.IsValid() && Run->Succeeded()
					? LOCTEXT("GenerationComplete", "Task generation is complete. Click Next to choose which tasks to keep.")
					: LOCTEXT("ExitCancels", "Exiting this window before the agent completes will cancel task generation.");
			})
			.ColorAndOpacity_Lambda([this]()
			{
				return Run.IsValid() && Run->Succeeded() ? FSlateColor(FStyleColors::Success) : FSlateColor(FStyleColors::Error);
			})
			.AutoWrapText(true)
		]

		// Agent log: its own scrollable panel, so a long run never pushes the buttons off the window
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 4.f)
		[
			ProtoBoyGenerateTasksDialog::MakeHeading(LOCTEXT("AgentLogHeading", "Agent log"))
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
			.Padding(8.f)
			[
				SAssignNew(LogScroll, SScrollBox)
				.Orientation(Orient_Vertical)
				.ScrollBarAlwaysVisible(true)
			]
		]

		// Cancel on the left, Finish on the right
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 12.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text_Lambda([this]() { return IsGenerating() ? LOCTEXT("CancelGenerating", "Cancel") : LOCTEXT("CloseGenerating", "Close"); })
				.ToolTipText_Lambda([this]()
				{
					return IsGenerating() ? LOCTEXT("CancelGeneratingTip", "Stop the agent. Asks first.")
						: HasPendingIdeas() ? LOCTEXT("DiscardTip", "Close without keeping any of the ideas. Asks first.")
						: FText::GetEmpty();
				})
				.OnClicked_Lambda([this]()
				{
					if (IsGenerating() ? ConfirmCancelGeneration() : (!HasPendingIdeas() || ConfirmDiscard()))
					{
						if (Run.IsValid())
						{
							Run->Cancel();
						}
						CloseWindow();
					}
					return FReply::Handled();
				})
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNullWidget::NullWidget
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
				.Text(LOCTEXT("Next", "Next"))
				.ToolTipText(LOCTEXT("NextTip", "Choose which task ideas to keep. Available once the agent is done."))
				.IsEnabled_Lambda([this]() { return Run.IsValid() && Run->Succeeded(); })
				.OnClicked_Lambda([this]() { ShowReview(); return FReply::Handled(); })
			]
		];
}

bool SProtoBoyGenerateTasksDialog::IsGenerating() const
{
	return Run.IsValid() && !Run->IsFinished();
}

void SProtoBoyGenerateTasksDialog::PollGeneration()
{
	if (!Run.IsValid() || !LogScroll.IsValid())
	{
		return;
	}
	Run->Tick();
	const TArray<FString>& Log = Run->GetLog();
	if (LogShown >= Log.Num())
	{
		return;
	}
	for (; LogShown < Log.Num(); ++LogShown)
	{
		LogScroll->AddSlot()
		.Padding(0.f, 1.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Log[LogShown]))
			.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
			.AutoWrapText(true)
		];
	}
	LogScroll->ScrollToEnd();
}

// --- Review ---------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> SProtoBoyGenerateTasksDialog::MakeReviewStep()
{
	const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	const IProtoBoyTaskProvider* Tasks = Subsystem ? Subsystem->GetTaskProvider() : nullptr;
	const FText Board = Tasks ? Tasks->GetDisplayName() : LOCTEXT("TheBoard", "the board");

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ReviewTitle", "Choose Tasks"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 8.f, 0.f, 8.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("ReviewWhat", "Tick the ideas to keep. Done adds them to their categories' To Do lists on {0}; the rest are discarded."), Board))
				.AutoWrapText(true)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(12.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					return FText::Format(LOCTEXT("ReviewCount", "{0} of {1} selected"),
						FText::AsNumber(SelectedIdeas.Num()), FText::AsNumber(Run.IsValid() ? Run->GetIdeas().Num() : 0));
				})
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		]

		// The ideas: filled by ShowReview
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
			.Padding(8.f)
			[
				SAssignNew(ReviewBox, SBox)
			]
		]

		// Adding progress, or what couldn't be added
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 8.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([this]() { return (Run.IsValid() && Run->IsAdding()) || !AddErrors.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Top)
			.Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(SCircularThrobber)
				.Radius(8.f)
				.Visibility_Lambda([this]() { return Run.IsValid() && Run->IsAdding() ? EVisibility::Visible : EVisibility::Collapsed; })
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text_Lambda([this]()
				{
					if (Run.IsValid() && Run->IsAdding())
					{
						return FText::Format(LOCTEXT("Adding", "Adding tasks to the board... ({0} of {1})"),
							FText::AsNumber(Run->GetNumAdded() + AddErrors.Num()), FText::AsNumber(NumToAdd));
					}
					return FText::Format(LOCTEXT("AddFailed", "Added {0} of {1}. Couldn't add:\n{2}"),
						FText::AsNumber(Run.IsValid() ? Run->GetNumAdded() : 0), FText::AsNumber(NumToAdd),
						FText::Join(FText::FromString(TEXT("\n")), AddErrors));
				})
				.ColorAndOpacity_Lambda([this]() { return Run.IsValid() && Run->IsAdding() ? FSlateColor::UseForeground() : FSlateColor(FStyleColors::Error); })
			]
		]

		// Cancel (discard) on the left, Done on the right
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 12.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text_Lambda([this]() { return bAddFinished ? LOCTEXT("CloseReview", "Close") : LOCTEXT("CancelReview", "Cancel"); })
				.ToolTipText_Lambda([this]()
				{
					return Run.IsValid() && Run->IsAdding() ? LOCTEXT("StopAddingTip", "Stop adding. Tasks already added stay on the board. Asks first.")
						: bAddFinished ? FText::GetEmpty()
						: LOCTEXT("CancelReviewTip", "Discard all of the ideas. Asks first.");
				})
				.OnClicked_Lambda([this]()
				{
					if (Run.IsValid() && Run->IsAdding() ? ConfirmCancelGeneration() : (bAddFinished || ConfirmDiscard()))
					{
						if (Run.IsValid())
						{
							Run->Cancel();
						}
						CloseWindow();
					}
					return FReply::Handled();
				})
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNullWidget::NullWidget
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
				.Text(LOCTEXT("Done", "Done"))
				.ToolTipText_Lambda([this]()
				{
					return SelectedIdeas.IsEmpty()
						? LOCTEXT("DoneNone", "Tick at least one idea to keep, or Cancel to discard them all.")
						: FText::Format(LOCTEXT("DoneTip", "Add the {0} ticked {0}|plural(one=task,other=tasks) to the board."), FText::AsNumber(SelectedIdeas.Num()));
				})
				.IsEnabled_Lambda([this]() { return !SelectedIdeas.IsEmpty() && Run.IsValid() && !Run->IsAdding() && !bAddFinished; })
				.OnClicked(this, &SProtoBoyGenerateTasksDialog::OnDoneClicked)
			]
		];
}

void SProtoBoyGenerateTasksDialog::ShowReview()
{
	if (!Run.IsValid() || !ReviewBox.IsValid())
	{
		return;
	}
	const TArray<FProtoBoyNewTask>& Ideas = Run->GetIdeas();
	SelectedIdeas.Reset();
	for (int32 Index = 0; Index < Ideas.Num(); ++Index)
	{
		SelectedIdeas.Add(Index);
	}

	// One group per picked category, in map order, then Uncategorized; empty groups are left out.
	TSharedRef<SScrollBox> List = SNew(SScrollBox).ScrollBarAlwaysVisible(true);
	TArray<FString> Groups = Run->GetCategoryTitles();
	Groups.Add(FString());
	for (const FString& Category : Groups)
	{
		TArray<int32> Indices;
		for (int32 Index = 0; Index < Ideas.Num(); ++Index)
		{
			if (Ideas[Index].Category == Category)
			{
				Indices.Add(Index);
			}
		}
		if (!Indices.IsEmpty())
		{
			List->AddSlot()
			.Padding(0.f, 0.f, 0.f, 10.f)
			[
				MakeReviewCategory(FText::FromString(Category.IsEmpty() ? FString(ProtoBoyTasks::UncategorizedName) : Category), Indices)
			];
		}
	}
	ReviewBox->SetContent(List);
	CurrentStep = EStep::Review;
}

ECheckBoxState SProtoBoyGenerateTasksDialog::GetCategoryState(const TArray<int32>& Indices) const
{
	int32 NumSelected = 0;
	for (const int32 Index : Indices)
	{
		NumSelected += SelectedIdeas.Contains(Index) ? 1 : 0;
	}
	return NumSelected == 0 ? ECheckBoxState::Unchecked
		: NumSelected == Indices.Num() ? ECheckBoxState::Checked
		: ECheckBoxState::Undetermined;
}

TSharedRef<SWidget> SProtoBoyGenerateTasksDialog::MakeReviewCategory(const FText& Title, const TArray<int32>& Indices)
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)

		// The category: ticks or clears all of its ideas
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 4.f)
		[
			SNew(SCheckBox)
			.IsEnabled_Lambda([this]() { return Run.IsValid() && !Run->IsAdding() && !bAddFinished; })
			.IsChecked_Lambda([this, Indices]() { return GetCategoryState(Indices); })
			.OnCheckStateChanged_Lambda([this, Indices](ECheckBoxState)
			{
				const bool bSelectAll = GetCategoryState(Indices) != ECheckBoxState::Checked;
				for (const int32 Index : Indices)
				{
					if (bSelectAll)
					{
						SelectedIdeas.Add(Index);
					}
					else
					{
						SelectedIdeas.Remove(Index);
					}
				}
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f, 0.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(Title)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text_Lambda([this, Indices]()
					{
						int32 NumSelected = 0;
						for (const int32 Index : Indices)
						{
							NumSelected += SelectedIdeas.Contains(Index) ? 1 : 0;
						}
						return FText::Format(LOCTEXT("CategoryCount", "{0} of {1}"), FText::AsNumber(NumSelected), FText::AsNumber(Indices.Num()));
					})
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]
		];

	// Its ideas, indented: title, then the agent's description
	const TArray<FProtoBoyNewTask>& Ideas = Run->GetIdeas();
	for (const int32 Index : Indices)
	{
		const FProtoBoyNewTask& Idea = Ideas[Index];
		Box->AddSlot()
		.AutoHeight()
		.Padding(24.f, 2.f, 0.f, 4.f)
		[
			SNew(SCheckBox)
			.IsEnabled_Lambda([this]() { return Run.IsValid() && !Run->IsAdding() && !bAddFinished; })
			.IsChecked_Lambda([this, Index]() { return SelectedIdeas.Contains(Index) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([this, Index](ECheckBoxState State)
			{
				if (State == ECheckBoxState::Checked)
				{
					SelectedIdeas.Add(Index);
				}
				else
				{
					SelectedIdeas.Remove(Index);
				}
			})
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(4.f, 0.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Idea.Title))
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(4.f, 2.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Visibility(Idea.Description.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
					.Text(FText::FromString(Idea.Description))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.AutoWrapText(true)
				]
			]
		];
	}
	return Box;
}

FReply SProtoBoyGenerateTasksDialog::OnDoneClicked()
{
	if (!Run.IsValid() || SelectedIdeas.IsEmpty())
	{
		return FReply::Handled();
	}
	// In the agent's order.
	TArray<int32> Indices = SelectedIdeas.Array();
	Indices.Sort();
	NumToAdd = Indices.Num();
	AddErrors.Reset();

	const TWeakPtr<SProtoBoyGenerateTasksDialog> WeakThis = SharedThis(this);
	Run->AddTasks(Indices, nullptr, [WeakThis](const TArray<FText>& Errors)
	{
		const TSharedPtr<SProtoBoyGenerateTasksDialog> This = WeakThis.Pin();
		if (!This.IsValid())
		{
			return;
		}
		This->bAddFinished = true;
		This->AddErrors = Errors;
		// All added: nothing left to look at here.
		if (Errors.IsEmpty())
		{
			This->CloseWindow();
		}
	});
	return FReply::Handled();
}

bool SProtoBoyGenerateTasksDialog::HasPendingIdeas() const
{
	return Run.IsValid() && Run->Succeeded() && !Run->IsAdding() && !bAddFinished;
}

bool SProtoBoyGenerateTasksDialog::ConfirmDiscard() const
{
	return FMessageDialog::Open(EAppMsgType::YesNo, EAppReturnType::No,
		FText::Format(LOCTEXT("DiscardConfirm", "Discard the {0} task {0}|plural(one=idea,other=ideas)?\n\nNothing is added to the board."), FText::AsNumber(Run.IsValid() ? Run->GetIdeas().Num() : 0)),
		LOCTEXT("DiscardTitle", "Discard Task Ideas")) == EAppReturnType::Yes;
}

bool SProtoBoyGenerateTasksDialog::ConfirmCancelGeneration() const
{
	const bool bAdding = Run.IsValid() && Run->IsAdding();
	return FMessageDialog::Open(EAppMsgType::YesNo, EAppReturnType::No,
		bAdding
			? LOCTEXT("CancelAddingConfirm", "Cancel generating tasks?\n\nProtoBoy stops adding tasks to the board. The ones already added stay there.")
			: LOCTEXT("CancelGeneratingConfirm", "Cancel generating tasks?\n\nThe agent stops and no tasks are added to the board."),
		LOCTEXT("CancelGeneratingTitle", "Cancel Generating Tasks")) == EAppReturnType::Yes;
}

void SProtoBoyGenerateTasksDialog::OnWindowCloseRequested(const TSharedRef<SWindow>& Window)
{
	// The close button is Cancel: ask while the agent works or tasks are being added, and before discarding ideas.
	if (!bClosing)
	{
		const bool bBusy = IsGenerating() || (Run.IsValid() && Run->IsAdding());
		if (bBusy ? !ConfirmCancelGeneration() : (HasPendingIdeas() && !ConfirmDiscard()))
		{
			return;
		}
		if (Run.IsValid())
		{
			Run->Cancel();
		}
	}
	FSlateApplication::Get().RequestDestroyWindow(Window);
}

void SProtoBoyGenerateTasksDialog::CloseWindow()
{
	bClosing = true;
	if (const TSharedPtr<SWindow> Window = ParentWindow.Pin())
	{
		Window->RequestDestroyWindow();
	}
}

#undef LOCTEXT_NAMESPACE
