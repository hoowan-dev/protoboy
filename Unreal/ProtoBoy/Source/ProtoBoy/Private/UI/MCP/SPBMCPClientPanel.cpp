#include "UI/MCP/SPBMCPClientPanel.h"

#include "HAL/PlatformApplicationMisc.h"
#include "MCP/PBMCPClientConfig.h"
#include "PBSubsystem.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "UI/PBStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SProtoBoyMCPClientPanel"

void SProtoBoyMCPClientPanel::Construct(const FArguments& InArgs, EProtoBoyLLMProvider InProvider)
{
	Provider = InProvider;

	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);

	Content->AddSlot()
	.AutoHeight()
	[
		MakeHeader()
	];

	Content->AddSlot()
	.AutoHeight()
	.Padding(0.f, 8.f, 0.f, 0.f)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("Intro", "Point an agent at ProtoBoy's MCP server. Write Config edits the agent's settings for you; or copy the setup and add it yourself."))
		.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		.AutoWrapText(true)
	];

	for (const EProtoBoyMCPClient Client : FProtoBoyMCPClientConfig::GetClients(Provider))
	{
		Content->AddSlot()
		.AutoHeight()
		.Padding(0.f, 12.f, 0.f, 0.f)
		[
			MakeClientCard(Client)
		];
	}

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			Content
		]
	];
}

TSharedRef<SWidget> SProtoBoyMCPClientPanel::MakeHeader()
{
	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.f, 0.f, 10.f, 0.f)
		[
			SNew(SImage)
			.Image(FProtoBoyStyle::Get().GetBrush(FProtoBoyMCPClientConfig::GetProviderIcon(Provider, true)))
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FProtoBoyMCPClientConfig::GetProviderName(Provider))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
		];
}

TSharedRef<SWidget> SProtoBoyMCPClientPanel::MakeClientCard(EProtoBoyMCPClient Client)
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
		.Padding(12.f)
		[
			SNew(SVerticalBox)

			// Name + configured status
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FProtoBoyMCPClientConfig::GetClientName(Client))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([Client]()
					{
						const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
						const bool bConfigured = Subsystem
							&& FProtoBoyMCPClientConfig::IsConfigured(Client, Subsystem->GetMCPServer().GetUrl());
						return bConfigured ? LOCTEXT("Configured", "Configured") : LOCTEXT("NotConfigured", "Not configured");
					})
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FProtoBoyMCPClientConfig::GetClientHint(Client))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.AutoWrapText(true)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FProtoBoyMCPClientConfig::GetConfigPath(Client)))
				.ToolTipText(LOCTEXT("ConfigPathTip", "Config file Write Config edits."))
				.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.AutoWrapText(true)
			]

			// Setup snippet (token masked; Copy puts the real one on the clipboard)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
				.Padding(8.f)
				[
					SNew(STextBlock)
					.Text_Lambda([this, Client]() { return FText::FromString(GetSnippet(Client, false)); })
					.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
					.AutoWrapText(true)
				]
			]

			// Actions
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("WriteConfig", "Write Config"))
					.OnClicked(this, &SProtoBoyMCPClientPanel::OnWriteConfigClicked, Client)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Copy", "Copy"))
					.ToolTipText(LOCTEXT("CopyTip", "Copy the setup, including the auth token."))
					.OnClicked(this, &SProtoBoyMCPClientPanel::OnCopyClicked, Client)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Test", "Test"))
					.OnClicked(this, &SProtoBoyMCPClientPanel::OnTestClicked, Client)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text_Lambda([this, Client]()
				{
					const FText* Message = Messages.Find(Client);
					return Message ? *Message : FText::GetEmpty();
				})
				.AutoWrapText(true)
			]
		];
}

FReply SProtoBoyMCPClientPanel::OnWriteConfigClicked(EProtoBoyMCPClient Client)
{
	UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	if (!Subsystem)
	{
		return FReply::Handled();
	}

	FProtoBoyMCPServer& Server = Subsystem->GetMCPServer();
	FText Error;
	Messages.Add(Client, FProtoBoyMCPClientConfig::WriteConfig(Client, Server.GetUrl(), Server.GetAuthToken(), Error)
		? LOCTEXT("Written", "Config written. Restart the agent to pick it up.")
		: Error);
	return FReply::Handled();
}

FReply SProtoBoyMCPClientPanel::OnCopyClicked(EProtoBoyMCPClient Client)
{
	FPlatformApplicationMisc::ClipboardCopy(*GetSnippet(Client, true));
	Messages.Add(Client, LOCTEXT("Copied", "Copied to clipboard."));
	return FReply::Handled();
}

FReply SProtoBoyMCPClientPanel::OnTestClicked(EProtoBoyMCPClient Client)
{
	// TODO: Check the server is running, the client config has a matching entry, and (once the server tracks
	//       client names from the MCP initialize request) whether this client has connected recently.
	Messages.Add(Client, LOCTEXT("TestNotImplemented", "Testing isn't implemented yet."));
	return FReply::Handled();
}

FString SProtoBoyMCPClientPanel::GetSnippet(EProtoBoyMCPClient Client, bool bRevealToken) const
{
	UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	if (!Subsystem)
	{
		return FString();
	}

	FProtoBoyMCPServer& Server = Subsystem->GetMCPServer();
	const FString Token = bRevealToken ? Server.GetAuthToken() : FString(TEXT("<token>"));
	return FProtoBoyMCPClientConfig::BuildSetupSnippet(Client, Server.GetUrl(), Token);
}

#undef LOCTEXT_NAMESPACE
