#include "UI/Tasks/Trello/SPBTrelloPanel.h"

#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#include "PBSettings.h"
#include "PBSubsystem.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "Tasks/Trello/PBTrelloConnector.h"
#include "UI/PBStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SProtoBoyTrelloPanel"

namespace ProtoBoyTrelloPanel
{
	bool IsBusy()
	{
		const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
		return Subsystem && Subsystem->GetTrelloConnector().GetState() != FProtoBoyTrelloConnector::EState::Idle;
	}

	bool IsConnected()
	{
		const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
		return Subsystem && Subsystem->GetTrelloConnector().IsConnected();
	}
}

void SProtoBoyTrelloPanel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SScrollBox)

		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MakeHeader()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Intro", "Connect opens Trello in your browser. Click Allow there and you'll be connected here automatically."))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.AutoWrapText(true)
			]

			// Status
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 16.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(this, &SProtoBoyTrelloPanel::GetStatusText)
				.ColorAndOpacity(this, &SProtoBoyTrelloPanel::GetStatusColor)
				.AutoWrapText(true)
			]

			// Actions
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			.Padding(0.f, 10.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
					.Visibility_Lambda([]() { return !ProtoBoyTrelloPanel::IsBusy() && !ProtoBoyTrelloPanel::IsConnected() ? EVisibility::Visible : EVisibility::Collapsed; })
					.Text(LOCTEXT("Connect", "Connect"))
					.OnClicked(this, &SProtoBoyTrelloPanel::OnConnectClicked)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Visibility_Lambda([]() { return ProtoBoyTrelloPanel::IsBusy() ? EVisibility::Visible : EVisibility::Collapsed; })
					.Text(LOCTEXT("Cancel", "Cancel"))
					.OnClicked(this, &SProtoBoyTrelloPanel::OnCancelClicked)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Visibility_Lambda([]() { return !ProtoBoyTrelloPanel::IsBusy() && ProtoBoyTrelloPanel::IsConnected() ? EVisibility::Visible : EVisibility::Collapsed; })
					.Text(LOCTEXT("Disconnect", "Disconnect"))
					.OnClicked(this, &SProtoBoyTrelloPanel::OnDisconnectClicked)
				]
			]

			// TODO: Board picker and list -> To Do / In Progress / Done mapping (team-wide, in UProtoBoySettings).

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 20.f, 0.f, 0.f)
			[
				MakeSetupDetails()
			]
		]
	];
}

TSharedRef<SWidget> SProtoBoyTrelloPanel::MakeHeader()
{
	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.f, 0.f, 10.f, 0.f)
		[
			SNew(SImage)
			.Image(FProtoBoyStyle::Get().GetBrush("ProtoBoy.Icon.Trello.Large"))
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Header", "Trello"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
		];
}

TSharedRef<SWidget> SProtoBoyTrelloPanel::MakeStep(int32 Number, const FText& Title, const TSharedRef<SWidget>& Body)
{
	return SNew(SHorizontalBox)

		// Step number
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Top)
		.Padding(0.f, 0.f, 10.f, 0.f)
		[
			SNew(SBox)
			.WidthOverride(16.f)
			[
				SNew(STextBlock)
				.Text(FText::AsNumber(Number))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(Title)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 3.f, 0.f, 0.f)
			[
				Body
			]
		];
}

TSharedRef<SWidget> SProtoBoyTrelloPanel::MakeSetupDetails()
{
	auto Paragraph = [](const FText& Text) -> TSharedRef<SWidget>
	{
		return SNew(STextBlock)
			.Text(Text)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true);
	};

	return SNew(SExpandableArea)
		// Open by default until a key is available, since Connect can't work without one.
		.InitiallyCollapsed(!FProtoBoyTrelloConnector::GetApiKey().IsEmpty())
		.AreaTitle(LOCTEXT("SetupTitle", "API key setup"))
		.BorderImage(FAppStyle::Get().GetBrush("NoBorder"))
		.BodyBorderImage(FAppStyle::Get().GetBrush("NoBorder"))
		.HeaderPadding(FMargin(0.f, 2.f))
		.Padding(FMargin(0.f, 6.f, 0.f, 0.f))
		.BodyContent()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
			.Padding(12.f)
			[
				SNew(SVerticalBox)

				// Which key is in use
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text_Lambda([]()
					{
						if (FProtoBoyTrelloConnector::GetApiKey().IsEmpty())
						{
							return LOCTEXT("KeyMissing", "No API key set yet. A key comes from a Trello Power-Up (Trello's name for an app registration). Create one once and the whole team can share its key:");
						}
						return GetDefault<UProtoBoySettings>()->TrelloApiKey.TrimStartAndEnd().IsEmpty()
							? LOCTEXT("KeyBuiltIn", "Using ProtoBoy's built-in Trello API key. To use your own instead:")
							: LOCTEXT("KeyProject", "Using the API key from Project Settings. To set up a different one:");
					})
					.ColorAndOpacity_Lambda([]()
					{
						return FProtoBoyTrelloConnector::GetApiKey().IsEmpty() ? FSlateColor(FStyleColors::Warning) : FSlateColor::UseForeground();
					})
					.AutoWrapText(true)
				]

				// 1. Create the Power-Up
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 12.f, 0.f, 0.f)
				[
					MakeStep(1, LOCTEXT("Step1Title", "Create a Power-Up"),
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							Paragraph(LOCTEXT("Step1Workspace", "You need a Trello Workspace you're an admin of (a personal one works)."))
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Left)
						.Padding(0.f, 4.f, 0.f, 0.f)
						[
							SNew(SHyperlink)
							.Text(LOCTEXT("Step1Link", "Open trello.com/power-ups/admin"))
							.ToolTipText(LOCTEXT("Step1LinkTip", "Opens Trello's Power-Up admin page in your browser."))
							.OnNavigate_Lambda([]() { FPlatformProcess::LaunchURL(TEXT("https://trello.com/power-ups/admin"), nullptr, nullptr); })
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.f, 4.f, 0.f, 0.f)
						[
							Paragraph(LOCTEXT("Step1Form", "Click New. Name it ProtoBoy, pick your Workspace, and fill in your contact details. The Iframe connector URL is only for Power-Ups that show UI inside Trello, which ProtoBoy doesn't: leave it blank if the form allows, otherwise enter any URL you control."))
						])
				]

				// 2. Generate the key
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 12.f, 0.f, 0.f)
				[
					MakeStep(2, LOCTEXT("Step2Title", "Generate the API key"),
						Paragraph(LOCTEXT("Step2Body", "Open the Power-Up's API key tab (sometimes called Trello Auth), not the OAuth 2.0 tab: ProtoBoy uses an API key and token, not OAuth 2.0. Click Generate a new API key and copy the key. Ignore the Secret on the same page: ProtoBoy never uses it, and it must not go into the project.")))
				]

				// 3. Allowed origin
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 12.f, 0.f, 0.f)
				[
					MakeStep(3, LOCTEXT("Step3Title", "Allow ProtoBoy's address"),
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							Paragraph(LOCTEXT("Step3Body", "On the same tab, add this under Allowed origins. Without it, Trello can't send your browser back to ProtoBoy after you click Allow."))
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.f, 6.f, 0.f, 0.f)
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(SBorder)
								.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
								.Padding(FMargin(8.f, 4.f))
								[
									SNew(STextBlock)
									.Text_Lambda([]() { return FText::FromString(FProtoBoyTrelloConnector::GetCallbackOrigin()); })
									.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
								]
							]

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(8.f, 0.f, 0.f, 0.f)
							[
								SNew(SButton)
								.Text(LOCTEXT("Copy", "Copy"))
								.OnClicked(this, &SProtoBoyTrelloPanel::OnCopyOriginClicked)
							]
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.f, 6.f, 0.f, 0.f)
						[
							Paragraph(LOCTEXT("Step3Port", "If you change Tasks Callback Port in Project Settings, update this origin to match."))
						])
				]

				// 4. Hand the key to ProtoBoy
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 12.f, 0.f, 0.f)
				[
					MakeStep(4, LOCTEXT("Step4Title", "Give the key to ProtoBoy"),
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							Paragraph(LOCTEXT("Step4Body", "Paste it into Project Settings > Plugins > ProtoBoy > Tasks > Trello Api Key. It's saved to DefaultProtoBoy.ini, which is fine to commit: an API key isn't a secret."))
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Left)
						.Padding(0.f, 6.f, 0.f, 0.f)
						[
							SNew(SButton)
							.Text(LOCTEXT("OpenProjectSettings", "Open Project Settings..."))
							.OnClicked(this, &SProtoBoyTrelloPanel::OnOpenProjectSettingsClicked)
						])
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 12.f, 0.f, 0.f)
				[
					Paragraph(LOCTEXT("ThenConnect", "Then click Connect above."))
				]
			]
		];
}

FReply SProtoBoyTrelloPanel::OnConnectClicked()
{
	StartError = FText::GetEmpty();
	if (FProtoBoyTrelloConnector* Connector = GetConnector())
	{
		Connector->BeginConnect(StartError);
	}
	return FReply::Handled();
}

FReply SProtoBoyTrelloPanel::OnCancelClicked()
{
	if (FProtoBoyTrelloConnector* Connector = GetConnector())
	{
		Connector->CancelConnect();
	}
	return FReply::Handled();
}

FReply SProtoBoyTrelloPanel::OnDisconnectClicked()
{
	StartError = FText::GetEmpty();
	if (FProtoBoyTrelloConnector* Connector = GetConnector())
	{
		Connector->Disconnect();
	}
	return FReply::Handled();
}

FReply SProtoBoyTrelloPanel::OnCopyOriginClicked()
{
	FPlatformApplicationMisc::ClipboardCopy(*FProtoBoyTrelloConnector::GetCallbackOrigin());
	return FReply::Handled();
}

FReply SProtoBoyTrelloPanel::OnOpenProjectSettingsClicked()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->ShowViewer("Project", "Plugins", GetDefault<UProtoBoySettings>()->GetSectionName());
	}
	return FReply::Handled();
}

FText SProtoBoyTrelloPanel::GetStatusText() const
{
	if (!StartError.IsEmpty())
	{
		return StartError;
	}

	const FProtoBoyTrelloConnector* Connector = GetConnector();
	if (!Connector)
	{
		return FText::GetEmpty();
	}

	switch (Connector->GetState())
	{
	case FProtoBoyTrelloConnector::EState::WaitingForBrowser:
		return LOCTEXT("Waiting", "Waiting for you to click Allow in the browser...");
	case FProtoBoyTrelloConnector::EState::Verifying:
		return LOCTEXT("Verifying", "Checking with Trello...");
	case FProtoBoyTrelloConnector::EState::Idle:
		break;
	}

	if (Connector->IsConnected())
	{
		const FProtoBoyTasksConnection& Connection = Connector->GetConnection();
		return Connection.TrelloFullName.IsEmpty()
			? FText::Format(LOCTEXT("ConnectedUser", "Connected as @{0}"), FText::FromString(Connection.TrelloUsername))
			: FText::Format(LOCTEXT("ConnectedFull", "Connected as {0} (@{1})"),
				FText::FromString(Connection.TrelloFullName), FText::FromString(Connection.TrelloUsername));
	}

	if (!Connector->GetLastError().IsEmpty())
	{
		return Connector->GetLastError();
	}
	return LOCTEXT("NotConnected", "Not connected.");
}

FSlateColor SProtoBoyTrelloPanel::GetStatusColor() const
{
	const FProtoBoyTrelloConnector* Connector = GetConnector();
	if (!StartError.IsEmpty() || (Connector && Connector->GetState() == FProtoBoyTrelloConnector::EState::Idle && !Connector->IsConnected() && !Connector->GetLastError().IsEmpty()))
	{
		return FStyleColors::Error;
	}
	if (Connector && Connector->GetState() == FProtoBoyTrelloConnector::EState::Idle && Connector->IsConnected())
	{
		return FStyleColors::Success;
	}
	return FSlateColor::UseForeground();
}

FProtoBoyTrelloConnector* SProtoBoyTrelloPanel::GetConnector()
{
	UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	return Subsystem ? &Subsystem->GetTrelloConnector() : nullptr;
}

#undef LOCTEXT_NAMESPACE
