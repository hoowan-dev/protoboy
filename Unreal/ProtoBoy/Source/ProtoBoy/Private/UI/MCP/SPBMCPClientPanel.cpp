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
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SProtoBoyMCPClientPanel"

void SProtoBoyMCPClientPanel::Construct(const FArguments& InArgs, EProtoBoyLLMProvider InProvider)
{
	Provider = InProvider;
	RefreshStatus();

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
		.Text(LOCTEXT("Intro", "Connect sets up the agent to use ProtoBoy's tools. Restart the agent afterwards."))
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

			// Name + status
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
					.Text_Lambda([this, Client]()
					{
						return IsConfigured(Client) ? LOCTEXT("Connected", "Connected") : LOCTEXT("NotConnected", "Not connected");
					})
					.ColorAndOpacity_Lambda([this, Client]()
					{
						return IsConfigured(Client) ? FSlateColor(FStyleColors::Success) : FSlateColor::UseSubduedForeground();
					})
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

			// Actions
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 10.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
					.Text_Lambda([this, Client]()
					{
						return IsConfigured(Client) ? LOCTEXT("Reconnect", "Reconnect") : LOCTEXT("Connect", "Connect");
					})
					.ToolTipText(FText::Format(LOCTEXT("ConnectTip", "Add ProtoBoy to {0}"),
						FText::FromString(FProtoBoyMCPClientConfig::GetConfigPath(Client))))
					.OnClicked(this, &SProtoBoyMCPClientPanel::OnConnectClicked, Client)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.Visibility_Lambda([this, Client]() { return IsConfigured(Client) ? EVisibility::Visible : EVisibility::Collapsed; })
					.Text(LOCTEXT("Disconnect", "Disconnect"))
					.OnClicked(this, &SProtoBoyMCPClientPanel::OnDisconnectClicked, Client)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Visibility_Lambda([this, Client]() { return Messages.Contains(Client) ? EVisibility::Visible : EVisibility::Collapsed; })
				.Text_Lambda([this, Client]()
				{
					const FText* Message = Messages.Find(Client);
					return Message ? *Message : FText::GetEmpty();
				})
				.ColorAndOpacity_Lambda([this, Client]()
				{
					return MessageIsError.FindRef(Client) ? FSlateColor(FStyleColors::Error) : FSlateColor::UseForeground();
				})
				.AutoWrapText(true)
			]

			// Manual setup, tucked away
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 10.f, 0.f, 0.f)
			[
				SNew(SExpandableArea)
				.InitiallyCollapsed(true)
				.AreaTitle(LOCTEXT("ManualSetup", "Manual setup"))
				.BorderImage(FAppStyle::Get().GetBrush("NoBorder"))
				.BodyBorderImage(FAppStyle::Get().GetBrush("NoBorder"))
				.HeaderPadding(FMargin(0.f, 2.f))
				.Padding(FMargin(0.f, 6.f, 0.f, 0.f))
				.BodyContent()
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(FProtoBoyMCPClientConfig::GetConfigPath(Client)))
						.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.AutoWrapText(true)
					]

					// Token masked; Copy puts the real one on the clipboard.
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 6.f, 0.f, 0.f)
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

					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Left)
					.Padding(0.f, 6.f, 0.f, 0.f)
					[
						SNew(SButton)
						.Text(LOCTEXT("Copy", "Copy"))
						.ToolTipText(LOCTEXT("CopyTip", "Copy the setup, including the auth token."))
						.OnClicked(this, &SProtoBoyMCPClientPanel::OnCopyClicked, Client)
					]
				]
			]
		];
}

FReply SProtoBoyMCPClientPanel::OnConnectClicked(EProtoBoyMCPClient Client)
{
	UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	if (!Subsystem)
	{
		return FReply::Handled();
	}

	FProtoBoyMCPServer& Server = Subsystem->GetMCPServer();
	FText Error;
	const bool bOk = Server.ConnectClient(Client, Error);
	RefreshStatus();

	if (!bOk)
	{
		Messages.Add(Client, Error);
	}
	else if (!Server.IsRunning())
	{
		Messages.Add(Client, LOCTEXT("ConnectedServerDown", "Connected, but ProtoBoy's MCP server isn't running yet, so the agent can't reach it."));
	}
	else
	{
		Messages.Add(Client, FText::Format(LOCTEXT("ConnectedOk", "Connected. Restart {0} to load ProtoBoy's tools."),
			FProtoBoyMCPClientConfig::GetClientName(Client)));
	}
	MessageIsError.Add(Client, !bOk);
	return FReply::Handled();
}

FReply SProtoBoyMCPClientPanel::OnDisconnectClicked(EProtoBoyMCPClient Client)
{
	UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	if (!Subsystem)
	{
		return FReply::Handled();
	}

	FText Error;
	const bool bOk = Subsystem->GetMCPServer().DisconnectClient(Client, Error);
	RefreshStatus();

	Messages.Add(Client, bOk ? LOCTEXT("Disconnected", "Disconnected.") : Error);
	MessageIsError.Add(Client, !bOk);
	return FReply::Handled();
}

FReply SProtoBoyMCPClientPanel::OnCopyClicked(EProtoBoyMCPClient Client)
{
	FPlatformApplicationMisc::ClipboardCopy(*GetSnippet(Client, true));
	Messages.Add(Client, LOCTEXT("Copied", "Copied to clipboard."));
	MessageIsError.Add(Client, false);
	return FReply::Handled();
}

void SProtoBoyMCPClientPanel::RefreshStatus()
{
	const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	const FString Url = Subsystem ? Subsystem->GetMCPServer().GetUrl() : FString();

	for (const EProtoBoyMCPClient Client : FProtoBoyMCPClientConfig::GetClients(Provider))
	{
		Configured.Add(Client, Subsystem && FProtoBoyMCPClientConfig::IsConfigured(Client, Url));
	}
}

bool SProtoBoyMCPClientPanel::IsConfigured(EProtoBoyMCPClient Client) const
{
	return Configured.FindRef(Client);
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
