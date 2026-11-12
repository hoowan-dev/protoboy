#include "UI/SPBDashboard.h"

#include "MCP/PBMCPClientConfig.h"
#include "Misc/MessageDialog.h"
#include "PBSubsystem.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "UI/PBStyle.h"
#include "UI/SPBConnectTasksDialog.h"
#include "UI/SPBConnectDocsDialog.h"
#include "UI/SPBConnectLLMDialog.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SProtoBoyDashboard"

void SProtoBoyDashboard::Construct(const FArguments& InArgs)
{
	// TODO: Status header, New ADR.
	ChildSlot
	[
		SNew(SBox)
		.Padding(16.f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Title", "ProtoBoy"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 32))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Tagline", "Build games, not slop"))
				.Font(FCoreStyle::GetDefaultFontStyle("Italic", 14))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 24.f, 0.f, 0.f)
			[
				MakeSection(LOCTEXT("MCPHeader", "MCP"),
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
						SNew(STextBlock)
						.Visibility_Lambda([]()
						{
							const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
							return Subsystem && Subsystem->GetMCPServer().HasConnectedClients() ? EVisibility::Visible : EVisibility::Collapsed;
						})
						.Text_Lambda([]()
						{
							TArray<FString> Names;
							for (const EProtoBoyMCPClient Client : UProtoBoySubsystem::Get()->GetMCPServer().GetConnection().ConfiguredClients)
							{
								Names.Add(FProtoBoyMCPClientConfig::GetClientName(Client).ToString());
							}
							return FText::Format(LOCTEXT("MCPSetUp", "Set up for: {0}"), FText::FromString(FString::Join(Names, TEXT(", "))));
						})
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
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
				MakeSection(LOCTEXT("DocsHeader", "Documentation"),
					SAssignNew(DocsSection, SBox))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 12.f, 0.f, 0.f)
			[
				MakeSection(LOCTEXT("TasksHeader", "Tasks"),
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TasksDescription", "Connect a Kanban-style board (Trello, Jira, Basecamp, ...)."))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.AutoWrapText(true)
					]

					// TODO: Show the chosen board once board selection exists.
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 4.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Text_Lambda([]()
						{
							const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
							if (!Subsystem || !Subsystem->GetTrelloConnector().IsConnected())
							{
								return LOCTEXT("TasksNotConnected", "No board connected.");
							}
							return FText::Format(LOCTEXT("TasksTrelloConnected", "Trello connected (@{0})"),
								FText::FromString(Subsystem->GetTrelloConnector().GetConnection().TrelloUsername));
						})
						.ColorAndOpacity_Lambda([]()
						{
							const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
							return Subsystem && Subsystem->GetTrelloConnector().IsConnected()
								? FSlateColor(FStyleColors::Success) : FSlateColor::UseSubduedForeground();
						})
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Left)
					.Padding(0.f, 8.f, 0.f, 0.f)
					[
						SNew(SButton)
						.Text_Lambda([]()
						{
							const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
							return Subsystem && Subsystem->GetTrelloConnector().IsConnected()
								? LOCTEXT("TasksSettingsConnected", "Task Board Settings...")
								: LOCTEXT("TasksSettings", "Connect Task Board...");
						})
						.OnClicked(this, &SProtoBoyDashboard::OnTasksSettingsClicked)
					])
			]
		]
	];

	RebuildDocsSection();

	if (UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get())
	{
		ConnectionChangedHandle = Subsystem->GetObsidianConnector().OnConnectionChanged.AddSP(
			this, &SProtoBoyDashboard::RebuildDocsSection);
	}
}

SProtoBoyDashboard::~SProtoBoyDashboard()
{
	if (UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get())
	{
		Subsystem->GetObsidianConnector().OnConnectionChanged.Remove(ConnectionChangedHandle);
	}
}

void SProtoBoyDashboard::RebuildDocsSection()
{
	const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	const FProtoBoyObsidianConnector* Obsidian = Subsystem ? &Subsystem->GetObsidianConnector() : nullptr;

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

	// Architecture doc: what agents use to decide where work belongs.
	const bool bHasArchitecture = Subsystem && Subsystem->GetDocGenerator().HasArchitectureDoc();
	Content->AddSlot()
	.AutoHeight()
	.Padding(0.f, 10.f, 0.f, 0.f)
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(bHasArchitecture
				? FText::Format(LOCTEXT("ArchitectureFound", "Architecture doc: {0}"), FText::FromString(FProtoBoyDocGenerator::ArchitectureDocPath))
				: LOCTEXT("ArchitectureMissing", "No Architecture doc yet. Agents use it to know which system owns new work."))
			.ColorAndOpacity(bHasArchitecture ? FSlateColor::UseForeground() : FSlateColor(FStyleColors::Warning))
			.AutoWrapText(true)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(8.f, 0.f, 0.f, 0.f)
		[
			SNew(SButton)
			.Visibility(bHasArchitecture ? EVisibility::Collapsed : EVisibility::Visible)
			.Text(LOCTEXT("CreateArchitecture", "Create Architecture Doc"))
			.ToolTipText(FText::Format(LOCTEXT("CreateArchitectureTip", "Writes {0} from a template (end goal, milestone, systems, rules) into {1}."),
				FText::FromString(FProtoBoyDocGenerator::ArchitectureDocPath),
				Subsystem ? Subsystem->GetDocsProvider().GetDisplayName() : FText::GetEmpty()))
			.OnClicked(this, &SProtoBoyDashboard::OnCreateArchitectureClicked)
		]
	];

	const bool bConnected = Obsidian && Obsidian->IsConnected();
	Content->AddSlot()
	.AutoHeight()
	.HAlign(HAlign_Left)
	.Padding(0.f, 8.f, 0.f, 0.f)
	[
		SNew(SButton)
		.Text(bConnected
			? LOCTEXT("ManageDocs", "Documentation Settings...")
			: LOCTEXT("ConnectDocs", "Connect to Documentation..."))
		.OnClicked(this, &SProtoBoyDashboard::OnConnectDocsClicked)
	];

	DocsSection->SetContent(Content);
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

FReply SProtoBoyDashboard::OnCreateArchitectureClicked()
{
	if (UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get())
	{
		FText Error;
		if (!Subsystem->GetDocGenerator().CreateArchitectureDoc(Error))
		{
			FMessageDialog::Open(EAppMsgType::Ok, Error, LOCTEXT("CreateArchitectureFailed", "Couldn't create the Architecture doc"));
		}
	}
	RebuildDocsSection();
	return FReply::Handled();
}

FReply SProtoBoyDashboard::OnConnectDocsClicked()
{
	SProtoBoyConnectDocsDialog::Open(AsShared());
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
