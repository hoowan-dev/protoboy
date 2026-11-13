#include "UI/SPBDashboard.h"

#include "ISettingsModule.h"
#include "Misc/ConfigCacheIni.h"
#include "Docs/PBDocGenerator.h"
#include "MCP/PBMCPClientConfig.h"
#include "Modules/ModuleManager.h"
#include "PBSettings.h"
#include "PBSubsystem.h"
#include "Sourcing/PBAssetSourcing.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "UI/PBStyle.h"
#include "UI/Docs/Map/SPBDocMapEditor.h"
#include "UI/Sourcing/SPBAssetSourcingPanel.h"
#include "UI/Tasks/SPBConnectTasksDialog.h"
#include "UI/Docs/SPBConnectDocsDialog.h"
#include "UI/MCP/SPBConnectLLMDialog.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
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
		MakeShared<FPage>(FPage{ LOCTEXT("ArchitecturePage", "Architecture"), LOCTEXT("ArchitecturePageTip", "Setup checklist and the map of the project's docs."), "Icons.Layout" }),
		MakeShared<FPage>(FPage{ LOCTEXT("DocumentationPage", "Documentation"), LOCTEXT("DocumentationPageTip", "The game's docs."), "Icons.Documentation" }),
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
				MakePage(1, MakeArchitectureSection())
			]

			+ SWidgetSwitcher::Slot()
			[
				MakePage(2, MakeDocumentationSection())
			]

			+ SWidgetSwitcher::Slot()
			[
				MakePage(3, MakeTasksSection())
			]
		]
	];

	PageList->SetSelection(Pages[ActivePage]);

	RebuildDocsSection();

	if (UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get())
	{
		ConnectionChangedHandle = Subsystem->GetObsidianConnector().OnConnectionChanged.AddSP(
			this, &SProtoBoyDashboard::RebuildDocsSection);
		// Creating the Game doc (from the doc map or setup) completes the docs setup step.
		DocMapChangedHandle = Subsystem->GetDocMap().OnChanged.AddSP(this, &SProtoBoyDashboard::RebuildDocsSection);
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
	// The docs folder may have changed, which changes whether the art direction doc exists.
	if (AssetSourcingPanel.IsValid())
	{
		AssetSourcingPanel->RefreshArtDirectionStatus();
	}

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

		const FString ArtDirection = ProtoBoyAssetSourcing::GetArtDirectionDocPath();
		OutDetail = FText::Format(LOCTEXT("StepSourcingDetail", "{0} Sources: {1}. Art direction: {2}."),
			Settings->bPreferSourcedAssets
				? LOCTEXT("SourcingPreferSourced", "Agents source assets instead of generating them with AI.")
				: LOCTEXT("SourcingAllowGenerated", "Agents may generate assets with AI."),
			Sources.IsEmpty() ? LOCTEXT("SourcesNone", "none enabled") : FText::Join(LOCTEXT("SourcesSeparator", ", "), Sources),
			ArtDirection.IsEmpty() ? LOCTEXT("ArtDirectionNone", "no note set (agents search the docs)") : FText::FromString(ArtDirection));
		return true;
	};

	SetupSteps = { AgentStep, DocsStep, TasksStep, SourcingStep };
}

TSharedRef<SWidget> SProtoBoyDashboard::MakeArchitectureSection()
{
	const TSharedRef<SWidget> Checks = SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text_Lambda([this]()
			{
				const int32 Done = CountDoneSetupSteps();
				return Done == SetupSteps.Num()
					? LOCTEXT("SetupAllDone", "Setup complete. Agents can plan and build within your architecture.")
					: FText::Format(LOCTEXT("SetupProgress", "Setup: {0} of {1} checks done"), FText::AsNumber(Done), FText::AsNumber(SetupSteps.Num()));
			})
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 8.f, 0.f, 0.f)
		[
			MakeSetupStep(LOCTEXT("StepAgent", "Connect an agent (MCP)"), AgentStep, &SProtoBoyDashboard::OnMCPSettingsClicked)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 6.f, 0.f, 0.f)
		[
			MakeSetupStep(LOCTEXT("StepDocs", "Connect documentation"), DocsStep, &SProtoBoyDashboard::OnConnectDocsClicked)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 6.f, 0.f, 0.f)
		[
			MakeSetupStep(LOCTEXT("StepTasks", "Connect a task board"), TasksStep, &SProtoBoyDashboard::OnTasksSettingsClicked)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 6.f, 0.f, 0.f)
		[
			MakeSetupStep(LOCTEXT("StepSourcing", "Asset sourcing"), SourcingStep, &SProtoBoyDashboard::OnAssetSourcingSettingsClicked,
				LOCTEXT("ChangeSourcing", "Change..."))
		];

	// The doc map is where the Game doc gets created, so it only waits on the other checks and on the docs being
	// connected; a missing Game doc doesn't lock it.
	auto IsSetupDone = [this]()
	{
		FText Ignored;
		const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
		return Subsystem && Subsystem->GetObsidianConnector().IsConnected() && AgentStep(Ignored) && TasksStep(Ignored);
	};

	const TSharedRef<SWidget> DocMap = SNew(SVerticalBox)

		// Why it's disabled
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 8.f)
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([IsSetupDone]() { return IsSetupDone() ? EVisibility::Collapsed : EVisibility::Visible; })

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("Icons.Lock"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("DocMapLocked", "Connect an agent, documentation and a task board above to edit the doc map. Agents plan against the docs and task board, so those need to be connected first."))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.AutoWrapText(true)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.IsEnabled_Lambda(IsSetupDone)
			[
				SNew(SProtoBoyDocMapEditor)
			]
		];

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			MakeSection(LOCTEXT("SetupChecksHeader", "Setup Checks"), Checks)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 12.f, 0.f, 0.f)
		[
			MakeSection(LOCTEXT("DocMapHeader", "Doc Map"), DocMap)
		];
}

TSharedRef<SWidget> SProtoBoyDashboard::MakeDocumentationSection()
{
	// TODO: Browse and search the docs.
	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> SProtoBoyDashboard::MakeTasksSection()
{
	// TODO: Show the task board's lists and the tasks linked to design docs.
	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> SProtoBoyDashboard::MakeSetupStep(const FText& Title, FSetupStepState GetState, FReply (SProtoBoyDashboard::*OnSetUp)(), const FText& ChangeText)
{
	auto IsDone = [GetState]() { FText Ignored; return GetState(Ignored); };

	return SNew(SHorizontalBox)

		// Check (done) or warning (to do)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Top)
		.Padding(0.f, 1.f, 8.f, 0.f)
		[
			SNew(SImage)
			.Image_Lambda([IsDone]()
			{
				return FAppStyle::Get().GetBrush(IsDone() ? "Icons.SuccessWithColor" : "Icons.XCircle");
			})
			.ColorAndOpacity_Lambda([IsDone]() { return IsDone() ? FSlateColor::UseForeground() : FSlateColor(FStyleColors::Error); })
			.DesiredSizeOverride(FVector2D(16.f, 16.f))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(Title)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 1.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text_Lambda([GetState]() { FText Detail; GetState(Detail); return Detail; })
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.AutoWrapText(true)
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(8.f, 0.f, 0.f, 0.f)
		[
			SNew(SButton)
			.Visibility_Lambda([IsDone, ChangeText]() { return IsDone() && ChangeText.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
			.Text(ChangeText.IsEmpty() ? LOCTEXT("SetUpStep", "Set Up...") : ChangeText)
			.OnClicked(this, OnSetUp)
		];
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

FReply SProtoBoyDashboard::OnAssetSourcingSettingsClicked()
{
	// Asset sourcing is edited on the Setup page.
	if (PageList.IsValid() && Pages.IsValidIndex(0))
	{
		PageList->SetSelection(Pages[0]);
	}
	return FReply::Handled();
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
