#include "UI/Docs/Obsidian/SPBObsidianPanel.h"

#include "DesktopPlatformModule.h"
#include "Docs/PBDocGenerator.h"
#include "Framework/Application/SlateApplication.h"
#include "IDesktopPlatform.h"
#include "Misc/Paths.h"
#include "PBSubsystem.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
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

		// Architecture doc (after connecting)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 16.f, 0.f, 0.f)
		[
			MakeArchitectureCard()
		]
	];

	Revalidate();
	RefreshArchitectureState();
}

TSharedRef<SWidget> SProtoBoyObsidianPanel::MakeArchitectureCard()
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
					return bHasArchitecture ? LOCTEXT("ArchitectureHeader", "Architecture doc")
						: LOCTEXT("ArchitectureHeaderSetup", "Next: set up the Architecture doc");
				})
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ArchitectureIntro", "The project's end goal, current milestone, systems and dependency rules. AI agents read it before designing or placing new work. Humans own it; agents only append."))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.AutoWrapText(true)
			]

			// Where it lives (team-wide)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 10.f, 0.f, 0.f)
			[
				MakeLabel(LOCTEXT("ArchitecturePathLabel", "Path in docs folder"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SAssignNew(ArchitecturePathBox, SEditableTextBox)
					.Text(FText::FromString(FProtoBoyDocGenerator::GetArchitectureDocPath()))
					.HintText(LOCTEXT("ArchitecturePathHint", "Architecture.md"))
					.ToolTipText(LOCTEXT("ArchitecturePathTip", "Any name or folder inside the docs folder. Point it at an existing note to use that note. Shared by the team (DefaultProtoBoy.ini)."))
					.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type) { ApplyArchitecturePath(Text.ToString()); })
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("BrowseArchitecture", "Browse..."))
					.ToolTipText(LOCTEXT("BrowseArchitectureTip", "Pick an existing note in the docs folder."))
					.OnClicked(this, &SProtoBoyObsidianPanel::OnBrowseArchitectureClicked)
				]
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
						if (!ArchitectureError.IsEmpty())
						{
							return ArchitectureError;
						}
						const FProtoBoyObsidianConnector* Connector = GetConnector();
						const FText Path = FText::FromString(Connector ? Connector->GetDocsFolder() / FProtoBoyDocGenerator::GetArchitectureDocPath() : FString());
						return bHasArchitecture
							? FText::Format(LOCTEXT("ArchitectureFound", "{0} is in the vault. Keep it current as the design changes."), Path)
							: FText::Format(LOCTEXT("ArchitectureMissing", "Create {0} from ProtoBoy's template, then fill it in in Obsidian."), Path);
					})
					.ColorAndOpacity_Lambda([this]()
					{
						return !ArchitectureError.IsEmpty() ? FSlateColor(FStyleColors::Error)
							: bHasArchitecture ? FSlateColor(FStyleColors::Success)
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
					.Visibility_Lambda([this]() { return bHasArchitecture ? EVisibility::Collapsed : EVisibility::Visible; })
					.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
					.Text(LOCTEXT("CreateArchitecture", "Create Architecture Doc"))
					.OnClicked(this, &SProtoBoyObsidianPanel::OnCreateArchitectureClicked)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(8.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.Visibility_Lambda([this]() { return bHasArchitecture ? EVisibility::Visible : EVisibility::Collapsed; })
					.Text(LOCTEXT("OpenArchitecture", "Open in Obsidian"))
					.OnClicked(this, &SProtoBoyObsidianPanel::OnOpenArchitectureClicked)
				]
			]
		];
}

void SProtoBoyObsidianPanel::ApplyArchitecturePath(const FString& Path)
{
	ArchitectureError = FText::GetEmpty();

	FString Normalized;
	if (!FProtoBoyDocGenerator::SetArchitectureDocPath(Path, Normalized, ArchitectureError))
	{
		return;
	}
	if (ArchitecturePathBox.IsValid())
	{
		ArchitecturePathBox->SetText(FText::FromString(Normalized));
	}
	RefreshArchitectureState();

	// The agent guide names the doc, so connected agents need the new path.
	if (UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get())
	{
		Subsystem->GetMCPServer().RefreshAgentGuides();
	}
}

FReply SProtoBoyObsidianPanel::OnBrowseArchitectureClicked()
{
	const FProtoBoyObsidianConnector* Connector = GetConnector();
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!Connector || !Connector->IsConnected() || !DesktopPlatform)
	{
		return FReply::Handled();
	}

	FString DocsRoot = FPaths::ConvertRelativePathToFull(Connector->GetDocsRoot());
	FPaths::NormalizeDirectoryName(DocsRoot);

	TArray<FString> Picked;
	const void* ParentHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared());
	if (!DesktopPlatform->OpenFileDialog(ParentHandle, LOCTEXT("BrowseArchitectureTitle", "Choose the Architecture doc").ToString(),
		DocsRoot, FString(), TEXT("Markdown (*.md)|*.md"), EFileDialogFlags::None, Picked) || Picked.IsEmpty())
	{
		return FReply::Handled();
	}

	FString File = FPaths::ConvertRelativePathToFull(Picked[0]);
	FPaths::NormalizeFilename(File);
	if (!File.StartsWith(DocsRoot + TEXT("/"), ESearchCase::IgnoreCase))
	{
		ArchitectureError = FText::Format(LOCTEXT("OutsideDocs", "Pick a note inside the docs folder ({0})."), FText::FromString(DocsRoot));
		return FReply::Handled();
	}

	ApplyArchitecturePath(File.Mid(DocsRoot.Len() + 1));
	return FReply::Handled();
}

void SProtoBoyObsidianPanel::RefreshArchitectureState()
{
	const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	bHasArchitecture = Subsystem && Subsystem->GetObsidianConnector().IsConnected() && Subsystem->GetDocGenerator().HasArchitectureDoc();
}

FReply SProtoBoyObsidianPanel::OnCreateArchitectureClicked()
{
	ArchitectureError = FText::GetEmpty();
	UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
	if (Subsystem && Subsystem->GetObsidianConnector().IsConnected())
	{
		Subsystem->GetDocGenerator().CreateArchitectureDoc(ArchitectureError);
	}
	RefreshArchitectureState();
	return FReply::Handled();
}

FReply SProtoBoyObsidianPanel::OnOpenArchitectureClicked()
{
	ArchitectureError = FText::GetEmpty();
	if (const FProtoBoyObsidianConnector* Connector = GetConnector())
	{
		Connector->OpenInObsidian(FProtoBoyDocGenerator::GetArchitectureDocPath(), ArchitectureError);
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
	ArchitectureError = FText::GetEmpty();
	RefreshArchitectureState();
	return FReply::Handled();
}

FReply SProtoBoyObsidianPanel::OnDisconnectClicked()
{
	if (FProtoBoyObsidianConnector* Connector = GetConnector())
	{
		Connector->Disconnect();
	}
	ArchitectureError = FText::GetEmpty();
	RefreshArchitectureState();
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
