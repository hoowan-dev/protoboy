#pragma once

#include "Docs/Obsidian/PBObsidianConnector.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;

/** Obsidian setup page of the "Connect to Documentation" window: pick a vault + docs folder, then Connect. */
class SProtoBoyObsidianPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProtoBoyObsidianPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	using FVaultPtr = TSharedPtr<FProtoBoyObsidianVault>;

	TSharedRef<SWidget> MakeHeader();
	TSharedRef<SWidget> MakeLabel(const FText& Text);

	/** Game doc card, shown once the vault is connected. */
	TSharedRef<SWidget> MakeGameDocCard();
	FReply OnCreateGameClicked();
	FReply OnOpenGameClicked();

	/** Re-check whether the Game doc exists (reads the doc map, so not done per frame). */
	void RefreshGameDocState();

	FReply OnBrowseClicked();
	FReply OnConnectClicked();
	FReply OnDisconnectClicked();
	void OnVaultPicked(FVaultPtr Vault, ESelectInfo::Type SelectInfo);

	/** Re-run as-you-type validation and update the status line. */
	void Revalidate();

	bool IsConnectedToCurrentInputs() const;

	FText GetStatusText() const;
	FSlateColor GetStatusColor() const;
	FText GetDocsRootPreview() const;
	FText GetConnectButtonText() const;

	FString GetVaultInput() const;
	FString GetFolderInput() const;

	static FProtoBoyObsidianConnector* GetConnector();

	TArray<FVaultPtr> KnownVaults;
	TSharedPtr<SComboBox<FVaultPtr>> VaultCombo;
	TSharedPtr<SEditableTextBox> VaultPathBox;
	TSharedPtr<SEditableTextBox> DocsFolderBox;

	bool bInputsValid = false;
	FText ValidationError;
	/** Set after a failed Connect; cleared on the next edit. */
	FText ConnectError;

	bool bHasGameDoc = false;
	/** Set when creating or opening the Game doc fails. */
	FText GameDocError;
};
