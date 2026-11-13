#include "UI/Docs/Obsidian/SPBObsidianPanel.h"

#include "DesktopPlatformModule.h"
#include "Docs/PBDocMap.h"
#include "Framework/Application/SlateApplication.h"
#include "IDesktopPlatform.h"
#include "Misc/Paths.h"
#include "PBSubsystem.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "UI/Docs/Map/SPBSetupDocsDialog.h"
#include "UI/PBStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SProtoBoyObsidianPanel"

void SProtoBoyObsidianPanel::Construct(const FArguments& InArgs)
{
	for (const FProtoBoyObsidianVault& Vault : FProtoBoyObsidianConnector::FindKnownVaults())
	{
		KnownVaults.Add(MakeShared<FProtoBoyObsidianVault>(Vault));
	}

	FString InitialVault;
	FString InitialFolder = FProtoBoyObsidianConnector::GetDefaultDocsFolder();
	if (const FProtoBoyObsidianConnector* Connector = GetConnector())
	{
		InitialVault = Connector->GetVaultPath();
		InitialFolder = Connector->GetDocsFolder();
	}
	if (InitialVault.IsEmpty() && KnownVaults.Num() == 1)
	{
		InitialVault = KnownVaults[0]->Path;
	}

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.f, 8.f, 0.f, 20.f)
		[
			MakeHeader()
		]

		// Vault
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			MakeLabel(LOCTEXT("VaultLabel", "Vault"))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SAssignNew(VaultPathBox, SEditableTextBox)
				.Text(FText::FromString(InitialVault))
				.HintText(LOCTEXT("VaultHint", "Path to an Obsidian vault folder"))
				.OnTextChanged_Lambda([this](const FText&) { ConnectError = FText::GetEmpty(); Revalidate(); })
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.f, 0.f, 0.f, 0.f)
			[
				SAssignNew(VaultCombo, SComboBox<FVaultPtr>)
				.OptionsSource(&KnownVaults)
				.IsEnabled(KnownVaults.Num() > 0)
				.ToolTipText(KnownVaults.Num() > 0
					? LOCTEXT("KnownTip", "Vaults Obsidian has opened on this machine.")
					: LOCTEXT("NoKnownTip", "No vaults found in Obsidian's settings. Use Browse."))
				.OnGenerateWidget_Lambda([](FVaultPtr Vault)
				{
					return SNew(STextBlock)
						.Text(FText::FromString(Vault->Name))
						.ToolTipText(FText::FromString(Vault->Path));
				})
				.OnSelectionChanged(this, &SProtoBoyObsidianPanel::OnVaultPicked)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("KnownVaults", "Known vaults"))
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Browse", "Browse..."))
				.OnClicked(this, &SProtoBoyObsidianPanel::OnBrowseClicked)
			]
		]

		// Docs folder
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 16.f, 0.f, 0.f)
		[
			MakeLabel(LOCTEXT("FolderLabel", "Docs folder in vault"))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			SAssignNew(DocsFolderBox, SEditableTextBox)
			.Text(FText::FromString(InitialFolder))
			.HintText(FText::FromString(FProtoBoyObsidianConnector::GetDefaultDocsFolder()))
			.OnTextChanged_Lambda([this](const FText&) { ConnectError = FText::GetEmpty(); Revalidate(); })
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(this, &SProtoBoyObsidianPanel::GetDocsRootPreview)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		]

		// Spacer
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SBox)
		]

		// Status + actions
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 12.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SProtoBoyObsidianPanel::GetStatusText)
				.ColorAndOpacity(this, &SProtoBoyObsidianPanel::GetStatusColor)
				.AutoWrapText(true)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Disconnect", "Disconnect"))
				.Visibility_Lambda([]()
				{
					const FProtoBoyObsidianConnector* Connector = GetConnector();
					return Connector && Connector->IsConnected() ? EVisibility::Visible : EVisibility::Collapsed;
				})
				.OnClicked(this, &SProtoBoyObsidianPanel::OnDisconnectClicked)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
				.Text(this, &SProtoBoyObsidianPanel::GetConnectButtonText)
				.IsEnabled_Lambda([this]() { return bInputsValid && !IsConnectedToCurrentInputs(); })
				.OnClicked(this, &SProtoBoyObsidianPanel::OnConnectClicked)
			]
		]

		// Game doc (after connecting)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 16.f, 0.f, 0.f)
		[
			MakeGameDocCard()
		]
	];

	Revalidate();
	RefreshGameDocState();
}

TSharedRef<SWidget> SProtoBoyObsidianPanel::MakeGameDocCard()
{
	auto IsVaultConnected = []()
	{
		const FProtoBoyObsidianConnector* Connector = GetConnector();
		return Connector && Connector->IsConnected();
	};

	return SNew(SBorder)
		.Visibility_Lambda([IsVaultConnected]() { return IsVaultConnected() ? EVisibility::Visible : EVisibility::Collapsed; })
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
		.Padding(12.f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					return bHasGameDoc ? LOCTEXT("GameHeader", "Game doc")
						: LOCTEXT("GameHeaderSetup", "Next: set up the Game doc");
				})
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("GameIntro", "The top of the project's docs: why the game exists (pitch, design pillars, non-goals), what it is (core loop, game flow), and an index of every other doc. AI agents read it before designing anything. Humans own it."))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.AutoWrapText(true)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Visibility_Lambda([this]() { return bHasGameDoc ? EVisibility::Collapsed : EVisibility::Visible; })
				.Text(LOCTEXT("GameExisting", "Already have a note for this? Add \"protoboy-layer: game\" to its properties instead, or map it from the dashboard's Doc Map."))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.AutoWrapText(true)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						if (!GameDocError.IsEmpty())
						{
							return GameDocError;
						}
						const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
						const FProtoBoyDocNode* Game = Subsystem ? Subsystem->GetDocMap().GetGame() : nullptr;
						if (bHasGameDoc && Game)
						{
							return FText::Format(LOCTEXT("GameFound", "{0} is in the vault. Keep it current as the design changes."), FText::FromString(Game->Path));
						}
						return LOCTEXT("GameMissing", "Set it up from a template (with a starting set of docs) or create it on its own, then fill it in in Obsidian, starting with the pitch and design pillars.");
					})
					.ColorAndOpacity_Lambda([this]()
					{
						return !GameDocError.IsEmpty() ? FSlateColor(FStyleColors::Error)
							: bHasGameDoc ? FSlateColor(FStyleColors::Success)
							: FSlateColor(FStyleColors::Warning);
					})
					.AutoWrapText(true)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(8.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.Visibility_Lambda([this]() { return bHasGameDoc ? EVisibility::Collapsed : EVisibility::Visible; })
					.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
					.Text(LOCTEXT("SetupFromTemplate", "Set Up from Template..."))
					.ToolTipText(LOCTEXT("SetupFromTemplateTip", "Pick a preset for your kind of game: the Game doc with suggested design pillars, and a starting set of docs below it."))
					.OnClicked_Lambda([this]()
					{
						GameDocError = FText::GetEmpty();
						SProtoBoySetupDocsDialog::Open(AsShared());
						RefreshGameDocState();
						return FReply::Handled();
					})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(6.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.Visibility_Lambda([this]() { return bHasGameDoc ? EVisibility::Collapsed : EVisibility::Visible; })
					.Text(LOCTEXT("CreateGame", "Create Game Doc"))
					.ToolTipText(LOCTEXT("CreateGameTip", "Just the Game doc, from its template."))
					.OnClicked(this, &SProtoBoyObsidianPanel::OnCreateGameClicked)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(8.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.Visibility_Lambda([this]() { return bHasGameDoc ? EVisibility::Visible : EVisibility::Collapsed; })
					.Text(LOCTEXT("OpenGame", "Open in Obsidian"))
					.OnClicked(this, &SProtoBoyObsidianPanel::OnOpenGameClicked)
				]
			]
		];
}

void SProtoBoyObsidianPanel::RefreshGameDocState()
{
	const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	bHasGameDoc = Subsystem && Subsystem->GetObsidianConnector().IsConnected() && Subsystem->GetDocMap().HasGameDoc();
}

FReply SProtoBoyObsidianPanel::OnCreateGameClicked()
{
	GameDocError = FText::GetEmpty();
	UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	if (Subsystem && Subsystem->GetObsidianConnector().IsConnected())
	{
		Subsystem->GetDocMap().CreateGameDoc(GameDocError);
	}
	RefreshGameDocState();
	return FReply::Handled();
}

FReply SProtoBoyObsidianPanel::OnOpenGameClicked()
{
	GameDocError = FText::GetEmpty();
	const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	const FProtoBoyDocNode* Game = Subsystem ? Subsystem->GetDocMap().GetGame() : nullptr;
	if (Game)
	{
		Subsystem->GetObsidianConnector().OpenInObsidian(Game->Path, GameDocError);
	}
	return FReply::Handled();
}

TSharedRef<SWidget> SProtoBoyObsidianPanel::MakeHeader()
{
	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.f, 0.f, 10.f, 0.f)
		[
			SNew(SImage)
			.Image(FProtoBoyStyle::Get().GetBrush("ProtoBoy.Icon.Obsidian.Large"))
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ObsidianHeader", "Obsidian"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
		];
}

TSharedRef<SWidget> SProtoBoyObsidianPanel::MakeLabel(const FText& Text)
{
	return SNew(STextBlock)
		.Text(Text)
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10));
}

FReply SProtoBoyObsidianPanel::OnBrowseClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return FReply::Handled();
	}

	const FString Current = GetVaultInput();
	const FString DefaultPath = FPaths::DirectoryExists(Current) ? Current : FPlatformProcess::UserDir();

	FString Picked;
	const void* ParentHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared());
	if (DesktopPlatform->OpenDirectoryDialog(ParentHandle, LOCTEXT("BrowseTitle", "Choose Obsidian vault").ToString(), DefaultPath, Picked))
	{
		VaultPathBox->SetText(FText::FromString(FPaths::ConvertRelativePathToFull(Picked)));
	}
	return FReply::Handled();
}

void SProtoBoyObsidianPanel::OnVaultPicked(FVaultPtr Vault, ESelectInfo::Type SelectInfo)
{
	if (Vault.IsValid())
	{
		VaultPathBox->SetText(FText::FromString(Vault->Path));
	}
}

FReply SProtoBoyObsidianPanel::OnConnectClicked()
{
	if (FProtoBoyObsidianConnector* Connector = GetConnector())
	{
		FText Error;
		ConnectError = Connector->Connect(GetVaultInput(), GetFolderInput(), Error) ? FText::GetEmpty() : Error;
	}
	GameDocError = FText::GetEmpty();
	RefreshGameDocState();
	return FReply::Handled();
}

FReply SProtoBoyObsidianPanel::OnDisconnectClicked()
{
	if (FProtoBoyObsidianConnector* Connector = GetConnector())
	{
		Connector->Disconnect();
	}
	GameDocError = FText::GetEmpty();
	RefreshGameDocState();
	return FReply::Handled();
}

void SProtoBoyObsidianPanel::Revalidate()
{
	bInputsValid = FProtoBoyObsidianConnector::ValidateVault(GetVaultInput(), ValidationError)
		&& FProtoBoyObsidianConnector::ValidateDocsFolder(GetFolderInput(), ValidationError);
}

bool SProtoBoyObsidianPanel::IsConnectedToCurrentInputs() const
{
	const FProtoBoyObsidianConnector* Connector = GetConnector();
	if (!Connector || !Connector->IsConnected())
	{
		return false;
	}
	return FPaths::IsSamePath(Connector->GetDocsRoot(), FProtoBoyObsidianConnector::MakeDocsRoot(GetVaultInput(), GetFolderInput()));
}

FText SProtoBoyObsidianPanel::GetStatusText() const
{
	if (!ConnectError.IsEmpty())
	{
		return ConnectError;
	}
	if (IsConnectedToCurrentInputs())
	{
		return FText::Format(LOCTEXT("Connected", "Connected to vault \"{0}\"."), FText::FromString(GetConnector()->GetVaultName()));
	}
	if (!bInputsValid)
	{
		return ValidationError;
	}
	const FProtoBoyObsidianConnector* Connector = GetConnector();
	if (Connector && Connector->IsConnected())
	{
		return LOCTEXT("PendingChange", "Settings changed. Reconnect to apply.");
	}
	return LOCTEXT("Ready", "Valid vault. Ready to connect.");
}

FSlateColor SProtoBoyObsidianPanel::GetStatusColor() const
{
	if (!ConnectError.IsEmpty())
	{
		return FStyleColors::Error;
	}
	if (IsConnectedToCurrentInputs())
	{
		return FStyleColors::Success;
	}
	return bInputsValid ? FSlateColor::UseForeground() : FStyleColors::Warning;
}

FText SProtoBoyObsidianPanel::GetDocsRootPreview() const
{
	const FString Vault = GetVaultInput();
	if (Vault.IsEmpty())
	{
		return FText::GetEmpty();
	}
	return FText::Format(LOCTEXT("Preview", "Docs will be written to: {0}"),
		FText::FromString(FProtoBoyObsidianConnector::MakeDocsRoot(Vault, GetFolderInput())));
}

FText SProtoBoyObsidianPanel::GetConnectButtonText() const
{
	const FProtoBoyObsidianConnector* Connector = GetConnector();
	return Connector && Connector->IsConnected() ? LOCTEXT("Reconnect", "Reconnect") : LOCTEXT("Connect", "Connect");
}

FString SProtoBoyObsidianPanel::GetVaultInput() const
{
	return VaultPathBox.IsValid() ? VaultPathBox->GetText().ToString().TrimStartAndEnd() : FString();
}

FString SProtoBoyObsidianPanel::GetFolderInput() const
{
	return DocsFolderBox.IsValid() ? DocsFolderBox->GetText().ToString().TrimStartAndEnd() : FString();
}

FProtoBoyObsidianConnector* SProtoBoyObsidianPanel::GetConnector()
{
	UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	return Subsystem ? &Subsystem->GetObsidianConnector() : nullptr;
}

#undef LOCTEXT_NAMESPACE
