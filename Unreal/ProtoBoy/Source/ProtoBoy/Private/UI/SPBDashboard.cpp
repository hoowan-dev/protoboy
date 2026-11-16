#include "UI/SPBDashboard.h"

#include "ISettingsModule.h"
#include "Misc/ConfigCacheIni.h"
#include "Docs/PBDocGenerator.h"
#include "MCP/PBMCPClientConfig.h"
#include "Modules/ModuleManager.h"
#include "PBSettings.h"
#include "PBSubsystem.h"
#include "Health/PBDocHealth.h"
#include "Sourcing/PBAssetSourcing.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "UI/PBStyle.h"
#include "Docs/PBDocMap.h"
#include "UI/Docs/Map/SPBDocMapEditor.h"
#include "UI/Docs/Map/SPBDocMapNode.h"
#include "UI/Health/SPBDocHealthPanel.h"
#include "UI/Sourcing/SPBAssetSourcingPanel.h"
#include "Tasks/PBTasksConnection.h"
#include "Tasks/Trello/PBTrelloClient.h"
#include "Tasks/Trello/PBTrelloConnector.h"
#include "UI/Tasks/SPBConnectTasksDialog.h"
#include "UI/Tasks/Trello/SPBTrelloSyncDialog.h"
#include "UI/Docs/SPBConnectDocsDialog.h"
#include "UI/MCP/SPBConnectLLMDialog.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "HAL/PlatformProcess.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SProtoBoyDashboard"

namespace ProtoBoyDashboard
{
	/** Per-user, per-project memory of the open page (EditorPerProjectUserSettings.ini). */
	const TCHAR* ConfigSection = TEXT("ProtoBoy.Dashboard");
	const TCHAR* ConfigActivePage = TEXT("ActivePage");

	bool IsTrelloConnected()
	{
		const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
		return Subsystem && Subsystem->GetTrelloConnector().IsConnected();
	}

	/** Board chosen and lists mapped (the team-wide setup in UProtoBoySettings). */
	bool IsBoardReady(FText& OutWhyNot)
	{
		const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
		return Subsystem && Subsystem->GetTrelloClient().IsReady(OutWhyNot);
	}
}

void SProtoBoyDashboard::Construct(const FArguments& InArgs)
{
	// TODO: Status header.
	Pages = {
		MakeShared<FPage>(FPage{ LOCTEXT("SetupPage", "Setup"), LOCTEXT("SetupPageTip", "Plugin settings and connections to agents, docs, task boards and asset sources."), "Icons.Settings" }),
		MakeShared<FPage>(FPage{ LOCTEXT("DocumentationPage", "Documentation"), LOCTEXT("DocumentationPageTip", "Setup checklist and the map of the project's docs."), "Icons.Documentation" }),
		MakeShared<FPage>(FPage{ LOCTEXT("TasksPage", "Tasks"), LOCTEXT("TasksPageTip", "The team's task board."), "Icons.Clipboard" }),
	};
	BuildSetupSteps();

	ActivePage = 0;
	GConfig->GetInt(ProtoBoyDashboard::ConfigSection, ProtoBoyDashboard::ConfigActivePage, ActivePage, GEditorPerProjectIni);
	ActivePage = Pages.IsValidIndex(ActivePage) ? ActivePage : 0;

	// One page per sidebar entry. Each scrolls on its own; lists inside (like the pillar graph) keep their own sizes.
	auto MakePage = [this](int32 Index, const TSharedRef<SWidget>& Body) -> TSharedRef<SWidget>
	{
		return SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SNew(SBox)
				.Padding(16.f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(Pages[Index]->Label)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 2.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Text(Pages[Index]->Description)
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.AutoWrapText(true)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 14.f, 0.f, 0.f)
					[
						Body
					]
				]
			];
	};

	ChildSlot
	[
		SNew(SHorizontalBox)

		// Sidebar: title and page list.
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
			.Padding(FMargin(0.f, 12.f))
			[
				SNew(SBox)
				.WidthOverride(190.f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(14.f, 0.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Title", "ProtoBoy"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 22))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(14.f, 2.f, 14.f, 0.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Tagline", "Build games, not slop"))
						.Font(FCoreStyle::GetDefaultFontStyle("Italic", 10))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.AutoWrapText(true)
					]

					+ SVerticalBox::Slot()
					.FillHeight(1.f)
					.Padding(0.f, 16.f, 0.f, 0.f)
					[
						SAssignNew(PageList, SListView<TSharedPtr<FPage>>)
						.ListItemsSource(&Pages)
						.SelectionMode(ESelectionMode::Single)
						.OnGenerateRow(this, &SProtoBoyDashboard::OnGeneratePageRow)
						.OnSelectionChanged(this, &SProtoBoyDashboard::OnPageSelected)
					]
				]
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SSeparator)
			.Orientation(Orient_Vertical)
			.Thickness(1.f)
		]

		// The selected page.
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		[
			SAssignNew(PageSwitcher, SWidgetSwitcher)
			.WidgetIndex_Lambda([this]() { return ActivePage; })

			+ SWidgetSwitcher::Slot()
			[
				MakePage(0, MakeSetupSection())
			]

			+ SWidgetSwitcher::Slot()
			[
				MakePage(1, MakeDocumentationSection())
			]

			+ SWidgetSwitcher::Slot()
			[
				MakePage(2, MakeTasksSection())
			]
		]
	];

	PageList->SetSelection(Pages[ActivePage]);

	RebuildDocsSection();
	RebuildTasksSection();

	if (UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get())
	{
		ConnectionChangedHandle = Subsystem->GetObsidianConnector().OnConnectionChanged.AddSP(
			this, &SProtoBoyDashboard::RebuildDocsSection);
		// Creating the Game doc (from the doc map or setup) completes the docs setup step; marking a doc a task
		// category changes the Tasks page. Connection changes refresh the map, so they arrive here too.
		DocMapChangedHandle = Subsystem->GetDocMap().OnChanged.AddSPLambda(this, [this]()
		{
			RebuildDocsSection();
			RebuildTasksSection();
		});
	}
}

TSharedRef<ITableRow> SProtoBoyDashboard::OnGeneratePageRow(TSharedPtr<FPage> Page, const TSharedRef<STableViewBase>& OwnerTable)
{
	const bool bSetup = Pages.Num() > 0 && Page == Pages[0];
	return SNew(STableRow<TSharedPtr<FPage>>, OwnerTable)
		.Padding(FMargin(14.f, 6.f))
		.ToolTipText(Page->Description)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush(Page->Icon))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Page->Label)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
			]

			// Setup shows its progress: a check when every step is done, else "done/total".
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				.Visibility(bSetup ? EVisibility::Visible : EVisibility::Collapsed)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.SuccessWithColor"))
					.DesiredSizeOverride(FVector2D(14.f, 14.f))
					.Visibility_Lambda([this]() { return CountDoneSetupSteps() == SetupSteps.Num() ? EVisibility::Visible : EVisibility::Collapsed; })
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return FText::Format(LOCTEXT("SetupCount", "{0}/{1}"), FText::AsNumber(CountDoneSetupSteps()), FText::AsNumber(SetupSteps.Num())); })
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Visibility_Lambda([this]() { return CountDoneSetupSteps() == SetupSteps.Num() ? EVisibility::Collapsed : EVisibility::Visible; })
				]
			]
		];
}

void SProtoBoyDashboard::OnPageSelected(TSharedPtr<FPage> Page, ESelectInfo::Type SelectInfo)
{
	const int32 Index = Pages.IndexOfByKey(Page);
	if (Index == INDEX_NONE)
	{
		// Keep a page selected (Ctrl+click would otherwise clear it).
		if (PageList.IsValid() && Pages.IsValidIndex(ActivePage))
		{
			PageList->SetSelection(Pages[ActivePage]);
		}
		return;
	}
	ActivePage = Index;
	GConfig->SetInt(ProtoBoyDashboard::ConfigSection, ProtoBoyDashboard::ConfigActivePage, ActivePage, GEditorPerProjectIni);
}

int32 SProtoBoyDashboard::CountDoneSetupSteps() const
{
	int32 Done = 0;
	for (const FSetupStepState& Step : SetupSteps)
	{
		FText Ignored;
		Done += Step(Ignored) ? 1 : 0;
	}
	return Done;
}

TSharedRef<SWidget> SProtoBoyDashboard::MakeSetupSection()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		[
			SNew(SButton)
			.Text(LOCTEXT("PluginSettings", "Plugin Settings..."))
			.ToolTipText(LOCTEXT("PluginSettingsTip", "Open Project Settings > Plugins > ProtoBoy (team-wide options, saved to DefaultProtoBoy.ini)."))
			.OnClicked(this, &SProtoBoyDashboard::OnOpenPluginSettingsClicked)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 12.f, 0.f, 0.f)
		[
			MakeSection(LOCTEXT("MCPHeader", "MCP"), AgentStep,
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("MCPDescription", "Connect your preferred agent."))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 4.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text_Lambda([]()
					{
						const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
						if (!Subsystem || !Subsystem->GetMCPServer().IsRunning())
						{
							return LOCTEXT("MCPStopped", "Server stopped.");
						}
						return FText::Format(LOCTEXT("MCPRunning", "Server running on {0}"),
							FText::FromString(Subsystem->GetMCPServer().GetUrl()));
					})
					.ColorAndOpacity_Lambda([]()
					{
						const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
						return Subsystem && Subsystem->GetMCPServer().IsRunning()
							? FSlateColor(FStyleColors::Success) : FSlateColor::UseSubduedForeground();
					})
				]

				// Agents that have talked to the server recently (names are what each agent reports, e.g. "claude-code").
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 6.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Visibility_Lambda([]()
					{
						const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
						return Subsystem && Subsystem->GetMCPServer().IsRunning() ? EVisibility::Visible : EVisibility::Collapsed;
					})
					.Text_Lambda([]()
					{
						const TArray<FString> Agents = UProtoBoySubsystem::Get()->GetMCPServer().GetRecentClients();
						if (Agents.IsEmpty())
						{
							return LOCTEXT("MCPNoAgents", "No agents connected.");
						}

						FString Lines;
						for (const FString& Agent : Agents)
						{
							Lines += FString::Printf(TEXT("%s• %s"), Lines.IsEmpty() ? TEXT("") : TEXT("\n"), *Agent);
						}
						return FText::Format(LOCTEXT("MCPAgents", "Connected agents:\n{0}"), FText::FromString(Lines));
					})
					.ColorAndOpacity_Lambda([]()
					{
						const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
						return Subsystem && Subsystem->GetMCPServer().GetConnectedClientCount() > 0
							? FSlateColor::UseForeground() : FSlateColor::UseSubduedForeground();
					})
				]

				// Agents this user has set up with Connect (they may not be running right now).
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 4.f, 0.f, 0.f)
				[
					MakeEnabledAgents()
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Left)
				.Padding(0.f, 8.f, 0.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("MCPSettings", "MCP Settings..."))
					.OnClicked(this, &SProtoBoyDashboard::OnMCPSettingsClicked)
				])
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 12.f, 0.f, 0.f)
		[
			MakeSection(LOCTEXT("DocsHeader", "Documentation"), DocsStep,
				SAssignNew(DocsSection, SBox))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 12.f, 0.f, 0.f)
		[
			MakeSection(LOCTEXT("TasksHeader", "Tasks"), TasksStep,
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("TasksDescription", "Connect a Kanban-style board (Trello, Jira, Basecamp, ...)."))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.AutoWrapText(true)
				]

				// Not connected
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 4.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Visibility_Lambda([]() { return ProtoBoyDashboard::IsTrelloConnected() ? EVisibility::Collapsed : EVisibility::Visible; })
					.Text(LOCTEXT("TasksNotConnected", "No board connected."))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]

				// Connected: Trello logo + account
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 4.f, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					.Visibility_Lambda([]() { return ProtoBoyDashboard::IsTrelloConnected() ? EVisibility::Visible : EVisibility::Collapsed; })

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.f, 0.f, 6.f, 0.f)
					[
						SNew(SImage)
						.Image(FProtoBoyStyle::Get().GetBrush("ProtoBoy.Icon.Trello"))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text_Lambda([]()
						{
							const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
							return Subsystem
								? FText::Format(LOCTEXT("TasksTrelloConnected", "Trello connected (@{0})"),
									FText::FromString(Subsystem->GetTrelloConnector().GetConnection().TrelloUsername))
								: FText::GetEmpty();
						})
						.ColorAndOpacity(FStyleColors::Success)
					]
				]

				// Connected: board status (or what's left to set up)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 4.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Visibility_Lambda([]() { return ProtoBoyDashboard::IsTrelloConnected() ? EVisibility::Visible : EVisibility::Collapsed; })
					.Text_Lambda([]()
					{
						FText WhyNot;
						if (!ProtoBoyDashboard::IsBoardReady(WhyNot))
						{
							return FText::Format(LOCTEXT("TasksBoardNotReady", "Setup needed: {0}"), WhyNot);
						}
						return FText::Format(LOCTEXT("TasksBoard", "Board \"{0}\": To Do, In Progress and Done are mapped."),
							FText::FromString(GetDefault<UProtoBoySettings>()->TrelloBoardName));
					})
					.ColorAndOpacity_Lambda([]()
					{
						FText WhyNot;
						return ProtoBoyDashboard::IsBoardReady(WhyNot) ? FSlateColor::UseSubduedForeground() : FSlateColor(FStyleColors::Warning);
					})
					.AutoWrapText(true)
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Left)
				.Padding(0.f, 8.f, 0.f, 0.f)
				[
					SNew(SButton)
					.Text_Lambda([]()
					{
						if (!ProtoBoyDashboard::IsTrelloConnected())
						{
							return LOCTEXT("TasksSettings", "Connect Task Board...");
						}
						FText WhyNot;
						return ProtoBoyDashboard::IsBoardReady(WhyNot)
							? LOCTEXT("TasksSettingsConnected", "Task Board Settings...")
							: LOCTEXT("TasksFinishSetup", "Finish Task Board Setup...");
					})
					.OnClicked(this, &SProtoBoyDashboard::OnTasksSettingsClicked)
				])
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 12.f, 0.f, 0.f)
		[
			MakeSection(LOCTEXT("AssetSourcingHeader", "Asset Sourcing (optional)"), SourcingStep, SAssignNew(AssetSourcingPanel, SProtoBoyAssetSourcingPanel))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 12.f, 0.f, 0.f)
		[
			MakeSection(LOCTEXT("DocHealthHeader", "Documentation Health Checks (optional)"), DocHealthStep, SNew(SProtoBoyDocHealthPanel))
		];
}

SProtoBoyDashboard::~SProtoBoyDashboard()
{
	if (UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get())
	{
		Subsystem->GetObsidianConnector().OnConnectionChanged.Remove(ConnectionChangedHandle);
		Subsystem->GetDocMap().OnChanged.Remove(DocMapChangedHandle);
	}
}

void SProtoBoyDashboard::RebuildDocsSection()
{
	const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	const FProtoBoyObsidianConnector* Obsidian = Subsystem ? &Subsystem->GetObsidianConnector() : nullptr;
	bHasGameDoc = Obsidian && Obsidian->IsConnected() && Subsystem->GetDocMap().HasGameDoc();

	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);

	if (Obsidian && Obsidian->IsConnected())
	{
		Content->AddSlot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(SImage)
				.Image(FProtoBoyStyle::Get().GetBrush("ProtoBoy.Icon.Obsidian"))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("ObsidianConnected", "Obsidian connected (vault \"{0}\")"),
					FText::FromString(Obsidian->GetVaultName())))
				.ColorAndOpacity(FStyleColors::Success)
			]
		];

		Content->AddSlot()
		.AutoHeight()
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Obsidian->GetDocsRoot()))
			.ToolTipText(LOCTEXT("DocsRootTip", "Folder ProtoBoy writes docs into."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		];

		// Setup state only; the setup itself lives in Connect to Documentation.
		Content->AddSlot()
		.AutoHeight()
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(bHasGameDoc
				? LOCTEXT("DocsSetupComplete", "Setup complete: the Game doc is in the vault.")
				: LOCTEXT("DocsSetupNeeded", "Setup needed: create the Game doc."))
			.ColorAndOpacity(bHasGameDoc ? FSlateColor::UseSubduedForeground() : FSlateColor(FStyleColors::Warning))
			.AutoWrapText(true)
		];
	}
	else
	{
		Content->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NotConnected", "No documentation platform connected."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];

	}

	const bool bConnected = Obsidian && Obsidian->IsConnected();
	const bool bSetUp = bConnected && bHasGameDoc;
	Content->AddSlot()
	.AutoHeight()
	.HAlign(HAlign_Left)
	.Padding(0.f, 8.f, 0.f, 0.f)
	[
		SNew(SButton)
		.Text(!bConnected ? LOCTEXT("ConnectDocs", "Connect to Documentation...")
			: bSetUp ? LOCTEXT("ManageDocs", "Documentation Settings...")
			: LOCTEXT("FinishDocs", "Finish Documentation Setup..."))
		.OnClicked(this, &SProtoBoyDashboard::OnConnectDocsClicked)
	];

	DocsSection->SetContent(Content);
}

void SProtoBoyDashboard::BuildSetupSteps()
{
	// Each step must be done for agents to plan within the architecture: an agent to talk to, docs to plan against,
	// and a board to plan into.
	AgentStep = [](FText& OutDetail)
	{
		const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
		if (!Subsystem)
		{
			return false;
		}
		const FProtoBoyMCPServer& Server = Subsystem->GetMCPServer();
		TArray<FString> Agents;
		for (const EProtoBoyMCPClient Client : Server.GetConnection().ConfiguredClients)
		{
			Agents.Add(FProtoBoyMCPClientConfig::GetClientName(Client).ToString());
		}

		if (Agents.IsEmpty())
		{
			OutDetail = LOCTEXT("StepAgentNone", "No agent connected yet.");
			return false;
		}
		if (!Server.IsRunning())
		{
			OutDetail = FText::Format(LOCTEXT("StepAgentStopped", "{0} set up, but the MCP server isn't running."), FText::FromString(FString::Join(Agents, TEXT(", "))));
			return false;
		}
		OutDetail = FText::Format(LOCTEXT("StepAgentDone", "{0}, server running."), FText::FromString(FString::Join(Agents, TEXT(", "))));
		return true;
	};

	DocsStep = [this](FText& OutDetail)
	{
		const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
		if (!Subsystem || !Subsystem->GetObsidianConnector().IsConnected())
		{
			OutDetail = LOCTEXT("StepDocsNone", "Obsidian isn't connected.");
			return false;
		}
		if (!bHasGameDoc)
		{
			OutDetail = LOCTEXT("StepDocsNoGame", "Obsidian connected; the Game doc isn't set up yet.");
			return false;
		}
		OutDetail = FText::Format(LOCTEXT("StepDocsDone", "Vault \"{0}\", Game doc in place."),
			FText::FromString(Subsystem->GetObsidianConnector().GetVaultName()));
		return true;
	};

	TasksStep = [](FText& OutDetail)
	{
		if (!ProtoBoyDashboard::IsTrelloConnected())
		{
			OutDetail = LOCTEXT("StepTasksNone", "No task board connected.");
			return false;
		}
		if (!ProtoBoyDashboard::IsBoardReady(OutDetail))
		{
			return false;
		}
		OutDetail = FText::Format(LOCTEXT("StepTasksDone", "Trello board \"{0}\", lists mapped."),
			FText::FromString(GetDefault<UProtoBoySettings>()->TrelloBoardName));
		return true;
	};

	// Asset sourcing is optional, so this step is always done; it reports the team's current choices.
	SourcingStep = [](FText& OutDetail)
	{
		const UProtoBoySettings* Settings = GetDefault<UProtoBoySettings>();

		TArray<FText> Sources;
		if (Settings->bEnableFabSource)
		{
			Sources.Add(LOCTEXT("SourceFab", "Fab"));
		}

		OutDetail = FText::Format(LOCTEXT("StepSourcingDetail", "{0} Sources: {1}. Style: from Content docs."),
			Settings->bPreferSourcedAssets
				? LOCTEXT("SourcingPreferSourced", "Agents source assets instead of generating them with AI.")
				: LOCTEXT("SourcingAllowGenerated", "Agents may generate assets with AI."),
			Sources.IsEmpty() ? LOCTEXT("SourcesNone", "none enabled") : FText::Join(LOCTEXT("SourcesSeparator", ", "), Sources));
		return true;
	};

	// Optional, so always green; the detail says whether it's on and whether revision control is connected.
	DocHealthStep = [](FText& OutDetail)
	{
		FText Status;
		const bool bConnected = ProtoBoyDocHealth::IsRevisionControlConnected(Status);
		OutDetail = FText::Format(LOCTEXT("StepDocHealthDetail", "{0} {1}"),
			ProtoBoyDocHealth::IsEnabled() ? LOCTEXT("DocHealthOn", "Health checks are enabled.")
				: bConnected ? LOCTEXT("DocHealthOff", "Health checks are off.")
				: LOCTEXT("DocHealthNeedsRC", "Health checks need revision control."),
			Status);
		return true;
	};

	SetupSteps = { AgentStep, DocsStep, TasksStep, SourcingStep, DocHealthStep };
}

TArray<FText> SProtoBoyDashboard::GetMissingSetup() const
{
	FText Ignored;
	const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	TArray<FText> Missing;
	if (!AgentStep(Ignored))
	{
		Missing.Add(LOCTEXT("MissingAgent", "an agent (MCP)"));
	}
	if (!Subsystem || !Subsystem->GetObsidianConnector().IsConnected())
	{
		Missing.Add(LOCTEXT("MissingDocs", "documentation"));
	}
	if (!TasksStep(Ignored))
	{
		Missing.Add(LOCTEXT("MissingTasks", "a task board"));
	}
	return Missing;
}

TSharedRef<SWidget> SProtoBoyDashboard::MakeSetupLock(const FText& LockedFormat)
{
	return SNew(SBorder)
		.Visibility_Lambda([this]() { return IsSetupDone() ? EVisibility::Collapsed : EVisibility::Visible; })
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
		.Padding(10.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("Icons.WarningWithColor"))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this, LockedFormat]()
				{
					return FText::Format(LockedFormat, FText::Join(LOCTEXT("MissingSeparator", ", "), GetMissingSetup()));
				})
				.ColorAndOpacity(FStyleColors::Warning)
				.AutoWrapText(true)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("GoToSetup", "Go to Setup"))
				.OnClicked_Lambda([this]()
				{
					if (PageList.IsValid() && Pages.IsValidIndex(0))
					{
						PageList->SetSelection(Pages[0]);
					}
					return FReply::Handled();
				})
			]
		];
}

TSharedRef<SWidget> SProtoBoyDashboard::MakeDocumentationSection()
{
	const TSharedRef<SWidget> DocMap = SNew(SVerticalBox)

		// Why it's disabled, and where to fix it
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 8.f)
		[
			MakeSetupLock(LOCTEXT("DocMapLocked", "Finish setup to use the doc map: connect {0} on the Setup page. Agents plan against the docs and task board, so those need to be connected first."))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.IsEnabled_Lambda([this]() { return IsSetupDone(); })
			[
				SNew(SProtoBoyDocMapEditor)
			]
		];

	return MakeSection(LOCTEXT("DocMapHeader", "Doc Map"), DocMap);
}

TSharedRef<SWidget> SProtoBoyDashboard::MakeTasksSection()
{
	const TSharedRef<SWidget> Body = SNew(SVerticalBox)

		// Sync with the connected task service, and whether the board matches the task categories
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 12.f)
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([this]() { return IsSetupDone() ? EVisibility::Visible : EVisibility::Collapsed; })

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.Text_Lambda([]()
				{
					const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
					const EProtoBoyTaskProvider Provider = Subsystem ? Subsystem->GetTrelloConnector().GetConnection().Provider : EProtoBoyTaskProvider::None;
					return FText::Format(LOCTEXT("SyncTasks", "Sync with {0}"), ProtoBoyTasks::GetProviderName(Provider));
				})
				.ToolTipText(LOCTEXT("SyncTasksTip", "Check that the board has a \"To Do - Uncategorized\" list and a \"To Do - <Category>\" list for each task category, and offer to fix it if not."))
				.IsEnabled_Lambda([this]() { return TaskSyncState != ETaskSyncState::Checking; })
				.OnClicked_Lambda([this]() { CheckTaskSync(/*bPromptIfOutOfSync*/ true); return FReply::Handled(); })
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(SCircularThrobber)
				.Radius(8.f)
				.Visibility_Lambda([this]() { return TaskSyncState == ETaskSyncState::Checking ? EVisibility::Visible : EVisibility::Collapsed; })
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(SImage)
				.Visibility_Lambda([this]()
				{
					return TaskSyncState == ETaskSyncState::InSync || TaskSyncState == ETaskSyncState::OutOfSync ? EVisibility::Visible : EVisibility::Collapsed;
				})
				.Image_Lambda([this]()
				{
					return FAppStyle::Get().GetBrush(TaskSyncState == ETaskSyncState::InSync ? "Icons.SuccessWithColor" : "Icons.XCircle");
				})
				.ToolTipText_Lambda([this]()
				{
					return TaskSyncState == ETaskSyncState::InSync
						? LOCTEXT("TasksInSync", "The board's To Do lists match the task categories.")
						: LOCTEXT("TasksOutOfSync", "Out of sync: the board's To Do lists don't match the task categories. Press Sync to see what's different and resync.");
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(10.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return GetLastTaskSyncText(); })
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			.Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Visibility_Lambda([this]() { return TaskSyncState == ETaskSyncState::Failed ? EVisibility::Visible : EVisibility::Collapsed; })
				.Text_Lambda([this]() { return TaskSyncError; })
				.ColorAndOpacity(FStyleColors::Error)
				.AutoWrapText(true)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			MakeSetupLock(LOCTEXT("TasksLocked", "Finish setup to use tasks: connect {0} on the Setup page."))
		]

		// To Do: each task category's list (filled by RebuildTasksSection)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 12.f)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return IsSetupDone() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				MakeSection(LOCTEXT("TasksToDoHeader", "To Do"), SAssignNew(TasksList, SBox))
			]
		]

		// In Progress: the board's mapped In Progress list (filled by RebuildTasksSection)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return IsSetupDone() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				MakeSection(LOCTEXT("TasksInProgressHeader", "In Progress"), SAssignNew(InProgressList, SBox))
			]
		];

	return Body;
}

TArray<FString> SProtoBoyDashboard::GetTaskCategoryNames() const
{
	TArray<FString> Names;
	if (const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get())
	{
		for (const FProtoBoyDocNode* Doc : Subsystem->GetDocMap().GetTaskCategories())
		{
			Names.Add(Doc->Title);
		}
	}
	return Names;
}

void SProtoBoyDashboard::CheckTaskSync(bool bPromptIfOutOfSync)
{
	const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	if (!Subsystem)
	{
		return;
	}
	const TArray<FString> Categories = GetTaskCategoryNames();
	TaskSyncState = ETaskSyncState::Checking;
	TaskSyncCategories = Categories;

	const TWeakPtr<SProtoBoyDashboard> WeakThis = SharedThis(this);
	Subsystem->GetTrelloClient().PlanCategorySync(Categories, [WeakThis, Categories, bPromptIfOutOfSync](const TProtoBoyTaskResult<FProtoBoyTrelloCategorySync>& Result)
	{
		const TSharedPtr<SProtoBoyDashboard> This = WeakThis.Pin();
		if (!This.IsValid())
		{
			return;
		}
		if (!Result.bOk)
		{
			This->TaskSyncState = ETaskSyncState::Failed;
			This->TaskSyncError = Result.Error;
			return;
		}

		// Each sync also refreshes the To Do tasks shown under the categories.
		This->TaskLists = Result.Value.CategoryLists;
		This->InProgressTasks = Result.Value.InProgressTasks;
		This->bTaskListsLoaded = true;

		// The categories changed while Trello answered: the in-sync result is about the old ones.
		if (This->GetTaskCategoryNames() != Categories)
		{
			This->TaskSyncState = ETaskSyncState::Unknown;
		}
		else if (Result.Value.IsInSync())
		{
			This->TaskSyncState = ETaskSyncState::InSync;
			if (UProtoBoySubsystem* Sub = UProtoBoySubsystem::Get())
			{
				Sub->GetTrelloConnector().SetLastTaskSync(FDateTime::UtcNow());
			}
		}
		else
		{
			This->TaskSyncState = ETaskSyncState::OutOfSync;
			if (bPromptIfOutOfSync)
			{
				// After a resync attempt, check again quietly: the icon, the last sync time and the tasks then show
				// what the board looks like now (cards from deleted lists are under Uncategorized).
				SProtoBoyTrelloSyncDialog::Open(This, Result.Value, [WeakThis](bool)
				{
					if (const TSharedPtr<SProtoBoyDashboard> Dashboard = WeakThis.Pin())
					{
						Dashboard->CheckTaskSync(/*bPromptIfOutOfSync*/ false);
					}
				});
			}
		}
		This->RebuildTasksSection();
	});
}

FText SProtoBoyDashboard::GetLastTaskSyncText() const
{
	const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	FDateTime When;
	if (!Subsystem || !FDateTime::ParseIso8601(*Subsystem->GetTrelloConnector().GetConnection().LastTaskSyncUtc, When))
	{
		return LOCTEXT("NeverSynced", "Never synced");
	}
	// AsDateTime takes UTC and shows it in the user's time zone.
	return FText::Format(LOCTEXT("LastSynced", "Last synced {0}"), FText::AsDateTime(When, EDateTimeStyle::Medium, EDateTimeStyle::Short));
}

const FProtoBoyTrelloCategorySync::FCategoryList* SProtoBoyDashboard::FindTaskList(const FString& Category) const
{
	return TaskLists.FindByPredicate([&Category](const FProtoBoyTrelloCategorySync::FCategoryList& List)
	{
		return List.Category.Equals(Category.TrimStartAndEnd(), ESearchCase::IgnoreCase);
	});
}

TSharedRef<SWidget> SProtoBoyDashboard::MakeTaskItem(const FProtoBoyTask& Task, bool bShowCategory)
{
	// Category chips: the task's labels that name a task category on the doc map.
	TSharedRef<SHorizontalBox> Chips = SNew(SHorizontalBox);
	if (bShowCategory)
	{
		const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
		const TArray<const FProtoBoyDocNode*> Categories = Subsystem ? Subsystem->GetDocMap().GetTaskCategories() : TArray<const FProtoBoyDocNode*>();
		for (const FString& Label : Task.Labels)
		{
			const FProtoBoyDocNode* const* Doc = Categories.FindByPredicate([&Label](const FProtoBoyDocNode* D) { return D->Title.Equals(Label.TrimStartAndEnd(), ESearchCase::IgnoreCase); });
			if (!Doc)
			{
				continue;
			}
			Chips->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(6.f, 0.f, 0.f, 0.f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(SProtoBoyDocMapNode::GetLayerColor((*Doc)->Layer))
				.Padding(FMargin(6.f, 1.f))
				.ToolTipText(FText::Format(LOCTEXT("CategoryChipTip", "From the \"{0}\" task category."), FText::FromString((*Doc)->Title)))
				[
					SNew(STextBlock)
					.Text(FText::FromString((*Doc)->Title))
					.TextStyle(FAppStyle::Get(), "SmallText")
					.ColorAndOpacity(FLinearColor::White)
				]
			];
		}
	}

	const FString Url = Task.Url;
	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Top)
		.Padding(0.f, 0.f, 6.f, 0.f)
		[
			SNew(STextBlock).Text(FText::FromString(TEXT("•")))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Top)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Task.Title))
				.ToolTipText(FText::FromString(Task.Description.Left(400)))
				.AutoWrapText(true)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				Chips
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Top)
		.Padding(8.f, 0.f, 0.f, 0.f)
		[
			SNew(SHyperlink)
			.Visibility(Url.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
			.Text(LOCTEXT("OpenTask", "Open"))
			.ToolTipText(FText::FromString(Url))
			.OnNavigate_Lambda([Url]() { FPlatformProcess::LaunchURL(*Url, nullptr, nullptr); })
		];
}

TSharedRef<SWidget> SProtoBoyDashboard::MakeTaskCategoryRow(const FString& Category, const FText& Detail, const FLinearColor& Color)
{
	const FProtoBoyTrelloCategorySync::FCategoryList* List = FindTaskList(Category);
	const FString Key = Category.TrimStartAndEnd().ToLower();

	// Count on the right of the header: the open cards in the category's To Do list.
	FText Count;
	FText CountTip;
	if (!bTaskListsLoaded)
	{
		Count = FText::FromString(TEXT("-"));
		CountTip = LOCTEXT("CountNotLoaded", "Press Sync to load the To Do tasks.");
	}
	else if (!List)
	{
		Count = LOCTEXT("CountNoList", "No list");
		CountTip = FText::Format(LOCTEXT("CountNoListTip", "The board has no \"{0}\" list yet. Press Sync to add it."),
			FText::FromString(FProtoBoyTrelloClient::GetCategoryListName(Category)));
	}
	else
	{
		Count = FText::Format(LOCTEXT("CountToDo", "{0} to do"), FText::AsNumber(List->Tasks.Num()));
		CountTip = FText::Format(LOCTEXT("CountToDoTip", "Open cards in \"{0}\" at the last sync."),
			FText::FromString(FProtoBoyTrelloClient::GetCategoryListName(Category)));
	}

	// The sublist, shown when the category is opened.
	TSharedRef<SVerticalBox> Tasks = SNew(SVerticalBox);
	auto AddNote = [&Tasks](const FText& Text)
	{
		Tasks->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(Text)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		];
	};
	if (!bTaskListsLoaded || !List)
	{
		AddNote(CountTip);
	}
	else if (List->Tasks.IsEmpty())
	{
		AddNote(LOCTEXT("NoToDoTasks", "No To Do tasks in this category."));
	}
	else
	{
		for (const FProtoBoyTask& Task : List->Tasks)
		{
			Tasks->AddSlot()
			.AutoHeight()
			.Padding(0.f, 2.f)
			[
				MakeTaskItem(Task)
			];
		}
	}

	return SNew(SExpandableArea)
		.InitiallyCollapsed(!ExpandedTaskCategories.Contains(Key))
		.OnAreaExpansionChanged_Lambda([this, Key](bool bExpanded)
		{
			if (bExpanded)
			{
				ExpandedTaskCategories.Add(Key);
			}
			else
			{
				ExpandedTaskCategories.Remove(Key);
			}
		})
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		.BodyBorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
		.HeaderPadding(FMargin(4.f, 2.f, 10.f, 2.f))
		.Padding(FMargin(30.f, 6.f, 10.f, 8.f))
		.HeaderContent()
		[
			SNew(SHorizontalBox)

			// Layer color, as on the doc map
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.f, 0.f, 0.f, 0.f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(Color)
				.Padding(FMargin(2.f, 0.f))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(10.f, 4.f, 8.f, 4.f)
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("Icons.Clipboard"))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			.Padding(0.f, 4.f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(Category))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(Detail)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(Count)
				.ToolTipText(CountTip)
				.ColorAndOpacity(List && !List->Tasks.IsEmpty() ? FSlateColor::UseForeground() : FSlateColor::UseSubduedForeground())
			]
		]
		.BodyContent()
		[
			Tasks
		];
}

void SProtoBoyDashboard::RebuildTasksSection()
{
	if (!TasksList.IsValid())
	{
		return;
	}
	const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	const FProtoBoyDocMap* Map = Subsystem ? &Subsystem->GetDocMap() : nullptr;

	// In doc map order, so categories read like the map does.
	const TArray<const FProtoBoyDocNode*> Categories = Map ? Map->GetTaskCategories() : TArray<const FProtoBoyDocNode*>();

	// A sync result is about the categories it was checked against; once they change it no longer says anything.
	if (TaskSyncState != ETaskSyncState::Checking && GetTaskCategoryNames() != TaskSyncCategories)
	{
		TaskSyncState = ETaskSyncState::Unknown;
	}

	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	if (Categories.IsEmpty())
	{
		Rows->AddSlot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 8.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoTaskCategories", "No task categories yet. On the doc map, right-click a doc and pick Task Category, or tick Task category under the selected doc."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		];
	}
	for (const FProtoBoyDocNode* Doc : Categories)
	{
		Rows->AddSlot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 4.f)
		[
			MakeTaskCategoryRow(Doc->Title,
				FText::Format(LOCTEXT("TaskCategoryDetail", "{0} doc · {1}"), ProtoBoyDocLayers::GetDisplayName(Doc->Layer), FText::FromString(Doc->Path)),
				SProtoBoyDocMapNode::GetLayerColor(Doc->Layer))
		];
	}

	// Tasks that belong to no doc, last.
	Rows->AddSlot()
	.AutoHeight()
	.Padding(0.f, 0.f, 0.f, 4.f)
	[
		MakeTaskCategoryRow(ProtoBoyTasks::UncategorizedName,
			LOCTEXT("UncategorizedDetail", "Tasks that belong to no doc"),
			FLinearColor(0.25f, 0.25f, 0.25f))
	];

	TasksList->SetContent(Rows);

	// In Progress
	if (InProgressList.IsValid())
	{
		TSharedRef<SVerticalBox> Items = SNew(SVerticalBox);
		auto AddNote = [&Items](const FText& Text)
		{
			Items->AddSlot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(Text)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.AutoWrapText(true)
			];
		};
		if (!bTaskListsLoaded)
		{
			AddNote(LOCTEXT("InProgressNotLoaded", "Press Sync to load the tasks in progress."));
		}
		else if (InProgressTasks.IsEmpty())
		{
			AddNote(LOCTEXT("NoInProgress", "Nothing in progress."));
		}
		else
		{
			for (const FProtoBoyTask& Task : InProgressTasks)
			{
				Items->AddSlot()
				.AutoHeight()
				.Padding(0.f, 2.f)
				[
					MakeTaskItem(Task, /*bShowCategory*/ true)
				];
			}
		}
		InProgressList->SetContent(Items);
	}
}

TSharedRef<SWidget> SProtoBoyDashboard::MakeSection(const FText& Title, const TSharedRef<SWidget>& Body)
{
	// Collapsible card styled like a Details panel category: tinted header bar, panel-colored body.
	return SNew(SExpandableArea)
		.AreaTitle(Title)
		.AreaTitleFont(FCoreStyle::GetDefaultFontStyle("Bold", 12))
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Header"))
		.BodyBorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
		.HeaderPadding(FMargin(8.f, 6.f))
		.Padding(FMargin(12.f, 10.f))
		.BodyContent()
		[
			Body
		];
}

TSharedRef<SWidget> SProtoBoyDashboard::MakeSection(const FText& Title, FSetupStepState Status, const TSharedRef<SWidget>& Body)
{
	// Same card, with a check (set up) or a red X (not yet) before the title. The tooltip says what's missing.
	return SNew(SExpandableArea)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Header"))
		.BodyBorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
		.HeaderPadding(FMargin(8.f, 6.f))
		.Padding(FMargin(12.f, 10.f))
		.HeaderContent()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(SImage)
				.Image_Lambda([Status]()
				{
					FText Ignored;
					return FAppStyle::Get().GetBrush(Status(Ignored) ? "Icons.SuccessWithColor" : "Icons.XCircle");
				})
				.ColorAndOpacity_Lambda([Status]()
				{
					FText Ignored;
					return Status(Ignored) ? FSlateColor::UseForeground() : FSlateColor(FStyleColors::Error);
				})
				.ToolTipText_Lambda([Status]()
				{
					FText Detail;
					Status(Detail);
					return Detail;
				})
				.DesiredSizeOverride(FVector2D(16.f, 16.f))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Title)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
			]
		]
		.BodyContent()
		[
			Body
		];
}

TSharedRef<SWidget> SProtoBoyDashboard::MakeEnabledAgents()
{
	auto IsConfigured = [](EProtoBoyMCPClient Client)
	{
		const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
		return Subsystem && Subsystem->GetMCPServer().GetConnection().ConfiguredClients.Contains(Client);
	};

	TSharedRef<SWrapBox> Chips = SNew(SWrapBox)
		.UseAllottedSize(true)
		.InnerSlotPadding(FVector2D(6.f, 4.f));

	// One chip per agent app (provider logo + app name), shown while this user has it set up with Connect. Built for
	// every client up front so connecting or disconnecting in the MCP dialog shows up without rebuilding.
	for (const EProtoBoyLLMProvider Provider : { EProtoBoyLLMProvider::Claude, EProtoBoyLLMProvider::Gemini })
	{
		for (const EProtoBoyMCPClient Client : FProtoBoyMCPClientConfig::GetClients(Provider))
		{
			Chips->AddSlot()
			[
				SNew(SBorder)
				.Visibility_Lambda([IsConfigured, Client]() { return IsConfigured(Client) ? EVisibility::Visible : EVisibility::Collapsed; })
				.BorderImage(FAppStyle::Get().GetBrush("Brushes.Header"))
				.Padding(FMargin(6.f, 3.f, 8.f, 3.f))
				.ToolTipText(FText::Format(LOCTEXT("AgentChipTip", "{0} is set up to use ProtoBoy's MCP server."), FProtoBoyMCPClientConfig::GetClientName(Client)))
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.f, 0.f, 6.f, 0.f)
					[
						SNew(SImage)
						.Image(FProtoBoyStyle::Get().GetBrush(FProtoBoyMCPClientConfig::GetProviderIcon(Provider)))
						.DesiredSizeOverride(FVector2D(18.f, 18.f))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FProtoBoyMCPClientConfig::GetClientName(Client))
					]
				]
			];
		}
	}

	return SNew(SVerticalBox)
		.Visibility_Lambda([]()
		{
			const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
			return Subsystem && Subsystem->GetMCPServer().HasConnectedClients() ? EVisibility::Visible : EVisibility::Collapsed;
		})

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("MCPSetUp", "Enabled agents:"))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			Chips
		];
}

FReply SProtoBoyDashboard::OnOpenPluginSettingsClicked()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->ShowViewer("Project", "Plugins", GetDefault<UProtoBoySettings>()->GetSectionName());
	}
	return FReply::Handled();
}

FReply SProtoBoyDashboard::OnConnectDocsClicked()
{
	SProtoBoyConnectDocsDialog::Open(AsShared());
	RebuildDocsSection();
	return FReply::Handled();
}

FReply SProtoBoyDashboard::OnTasksSettingsClicked()
{
	SProtoBoyConnectTasksDialog::Open(AsShared());
	return FReply::Handled();
}

FReply SProtoBoyDashboard::OnMCPSettingsClicked()
{
	SProtoBoyConnectLLMDialog::Open(AsShared());
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
