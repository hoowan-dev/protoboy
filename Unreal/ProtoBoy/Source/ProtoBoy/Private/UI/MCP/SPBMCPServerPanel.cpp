#include "UI/MCP/SPBMCPServerPanel.h"

#include "HAL/PlatformApplicationMisc.h"
#include "ISettingsModule.h"
#include "MCP/PBMCPServer.h"
#include "Modules/ModuleManager.h"
#include "PBSettings.h"
#include "PBSubsystem.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SProtoBoyMCPServerPanel"

namespace
{
	/** Edit the committed project settings and write them back to DefaultProtoBoy.ini. */
	void EditSettings(TFunctionRef<void(UProtoBoySettings&)> Edit)
	{
		UProtoBoySettings* Settings = GetMutableDefault<UProtoBoySettings>();
		Edit(*Settings);
		Settings->TryUpdateDefaultConfigFile();
	}
}

void SProtoBoyMCPServerPanel::Construct(const FArguments& InArgs)
{
	TSharedRef<SVerticalBox> ToolList = SNew(SVerticalBox);
	if (const FProtoBoyMCPServer* Server = GetServer())
	{
		for (const FProtoBoyMCPTool& Tool : Server->GetTools())
		{
			ToolList->AddSlot().AutoHeight().Padding(0.f, 2.f)[ MakeToolRow(Tool) ];
		}
	}

	ChildSlot
	[
		SNew(SScrollBox)

		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Header", "MCP Server"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Intro", "Agents connect to this local server to call ProtoBoy tools. It only accepts connections from this machine."))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.AutoWrapText(true)
			]

			// Status
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 16.f, 0.f, 0.f)
			[
				MakeStatusRow()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 6.f, 0.f, 0.f)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([]()
				{
					return GetDefault<UProtoBoySettings>()->bEnableMCPServer ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([](ECheckBoxState State)
				{
					EditSettings([State](UProtoBoySettings& S) { S.bEnableMCPServer = State == ECheckBoxState::Checked; });
				})
				.ToolTipText(LOCTEXT("StartWithEditorTip", "Team-wide setting. The server also starts for anyone who has connected an agent."))
				[
					SNew(STextBlock).Text(LOCTEXT("StartWithEditor", "Start with the editor for everyone"))
				]
			]

			// Auth token
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 16.f, 0.f, 4.f)
			[
				MakeLabel(LOCTEXT("TokenLabel", "Auth token"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MakeTokenRow()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("TokenHint", "Stored per user in Saved/ProtoBoy. Connected agents are updated automatically when it's regenerated (restart them afterwards)."))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.AutoWrapText(true)
			]

			// Tool permissions
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 16.f, 0.f, 4.f)
			[
				MakeLabel(LOCTEXT("ToolsLabel", "Tools"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([]()
				{
					return GetDefault<UProtoBoySettings>()->bAllowMCPWriteTools ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([](ECheckBoxState State)
				{
					EditSettings([State](UProtoBoySettings& S) { S.bAllowMCPWriteTools = State == ECheckBoxState::Checked; });
				})
				[
					SNew(STextBlock).Text(LOCTEXT("AllowWrites", "Allow tools that modify the project"))
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(20.f, 2.f, 0.f, 0.f)
			[
				SNew(SCheckBox)
				.IsEnabled_Lambda([]() { return GetDefault<UProtoBoySettings>()->bAllowMCPWriteTools; })
				.IsChecked_Lambda([]()
				{
					return GetDefault<UProtoBoySettings>()->bConfirmMCPWriteTools ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([](ECheckBoxState State)
				{
					EditSettings([State](UProtoBoySettings& S) { S.bConfirmMCPWriteTools = State == ECheckBoxState::Checked; });
				})
				[
					SNew(STextBlock).Text(LOCTEXT("ConfirmWrites", "Ask before each change"))
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
				.Padding(8.f)
				[
					ToolList
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			.Padding(0.f, 12.f, 0.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("OpenProjectSettings", "Open Project Settings..."))
				.ToolTipText(LOCTEXT("OpenProjectSettingsTip", "Port and other team-wide MCP options (saved to DefaultProtoBoy.ini)."))
				.OnClicked(this, &SProtoBoyMCPServerPanel::OnOpenProjectSettingsClicked)
			]
		]
	];
}

TSharedRef<SWidget> SProtoBoyMCPServerPanel::MakeStatusRow()
{
	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(this, &SProtoBoyMCPServerPanel::GetStatusText)
			.ColorAndOpacity(this, &SProtoBoyMCPServerPanel::GetStatusColor)
			.AutoWrapText(true)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(8.f, 0.f, 0.f, 0.f)
		[
			SNew(SButton)
			.Text_Lambda([]()
			{
				const FProtoBoyMCPServer* Server = GetServer();
				return Server && Server->IsRunning() ? LOCTEXT("Stop", "Stop") : LOCTEXT("Start", "Start");
			})
			.OnClicked(this, &SProtoBoyMCPServerPanel::OnStartStopClicked)
		];
}

TSharedRef<SWidget> SProtoBoyMCPServerPanel::MakeTokenRow()
{
	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
			.Text_Lambda([this]()
			{
				FProtoBoyMCPServer* Server = GetServer();
				if (!Server)
				{
					return FText::GetEmpty();
				}
				const FString& Token = Server->GetAuthToken();
				return FText::FromString(bShowToken ? Token : FString::ChrN(Token.Len(), TEXT('*')));
			})
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(8.f, 0.f, 0.f, 0.f)
		[
			SNew(SButton)
			.Text_Lambda([this]() { return bShowToken ? LOCTEXT("Hide", "Hide") : LOCTEXT("Show", "Show"); })
			.OnClicked_Lambda([this]() { bShowToken = !bShowToken; return FReply::Handled(); })
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4.f, 0.f, 0.f, 0.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("Copy", "Copy"))
			.OnClicked(this, &SProtoBoyMCPServerPanel::OnCopyTokenClicked)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4.f, 0.f, 0.f, 0.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("Regenerate", "Regenerate"))
			.OnClicked(this, &SProtoBoyMCPServerPanel::OnRegenerateTokenClicked)
		];
}

TSharedRef<SWidget> SProtoBoyMCPServerPanel::MakeToolRow(const FProtoBoyMCPTool& Tool)
{
	const FName Name = Tool.Name;
	const bool bWrites = Tool.bWrites;

	return SNew(SCheckBox)
		// Write tools can't be turned on while writes are disallowed project-wide.
		.IsEnabled_Lambda([bWrites]() { return !bWrites || GetDefault<UProtoBoySettings>()->bAllowMCPWriteTools; })
		.IsChecked_Lambda([Name]()
		{
			return GetDefault<UProtoBoySettings>()->DisabledMCPTools.Contains(Name) ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
		})
		.OnCheckStateChanged_Lambda([Name](ECheckBoxState State)
		{
			EditSettings([Name, State](UProtoBoySettings& S)
			{
				if (State == ECheckBoxState::Checked)
				{
					S.DisabledMCPTools.Remove(Name);
				}
				else
				{
					S.DisabledMCPTools.AddUnique(Name);
				}
			});
		})
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromName(Name))
				.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(6.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Visibility(bWrites ? EVisibility::Visible : EVisibility::Collapsed)
				.Text(LOCTEXT("WritesTag", "WRITES"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
				.ColorAndOpacity(FStyleColors::Warning)
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			.Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(Tool.Description)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		];
}

TSharedRef<SWidget> SProtoBoyMCPServerPanel::MakeLabel(const FText& Text)
{
	return SNew(STextBlock)
		.Text(Text)
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10));
}

FReply SProtoBoyMCPServerPanel::OnStartStopClicked()
{
	FProtoBoyMCPServer* Server = GetServer();
	if (!Server)
	{
		return FReply::Handled();
	}

	StartError = FText::GetEmpty();
	if (Server->IsRunning())
	{
		Server->Stop();
	}
	else if (!Server->Start(GetDefault<UProtoBoySettings>()->MCPPort))
	{
		StartError = LOCTEXT("StartFailed", "Couldn't start the server (not implemented yet).");
	}
	return FReply::Handled();
}

FReply SProtoBoyMCPServerPanel::OnCopyTokenClicked()
{
	if (FProtoBoyMCPServer* Server = GetServer())
	{
		FPlatformApplicationMisc::ClipboardCopy(*Server->GetAuthToken());
	}
	return FReply::Handled();
}

FReply SProtoBoyMCPServerPanel::OnRegenerateTokenClicked()
{
	// TODO: Confirm first if any clients are configured, since they'll stop working.
	if (FProtoBoyMCPServer* Server = GetServer())
	{
		Server->RegenerateAuthToken();
	}
	return FReply::Handled();
}

FReply SProtoBoyMCPServerPanel::OnOpenProjectSettingsClicked()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->ShowViewer("Project", "Plugins", GetDefault<UProtoBoySettings>()->GetSectionName());
	}
	return FReply::Handled();
}

FText SProtoBoyMCPServerPanel::GetStatusText() const
{
	if (!StartError.IsEmpty())
	{
		return StartError;
	}

	const FProtoBoyMCPServer* Server = GetServer();
	if (!Server || !Server->IsRunning())
	{
		return FText::Format(LOCTEXT("Stopped", "Stopped. Will listen on {0}"), FText::FromString(Server ? Server->GetUrl() : FString()));
	}
	return FText::Format(LOCTEXT("Running", "Running on {0} ({1} connected)"),
		FText::FromString(Server->GetUrl()), FText::AsNumber(Server->GetConnectedClientCount()));
}

FSlateColor SProtoBoyMCPServerPanel::GetStatusColor() const
{
	if (!StartError.IsEmpty())
	{
		return FStyleColors::Error;
	}
	const FProtoBoyMCPServer* Server = GetServer();
	return Server && Server->IsRunning() ? FStyleColors::Success : FSlateColor::UseSubduedForeground();
}

FProtoBoyMCPServer* SProtoBoyMCPServerPanel::GetServer()
{
	UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	return Subsystem ? &Subsystem->GetMCPServer() : nullptr;
}

#undef LOCTEXT_NAMESPACE
