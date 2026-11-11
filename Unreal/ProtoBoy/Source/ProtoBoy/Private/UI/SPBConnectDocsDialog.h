#pragma once

#include "Widgets/SCompoundWidget.h"

class SWidgetSwitcher;

/** Documentation platforms ProtoBoy can connect to. Values double as SWidgetSwitcher slot indices. */
enum class EProtoBoyDocPlatform : uint8
{
	None = 0,
	Obsidian = 1,
};

/**
 * "Connect to Documentation" window content: a "Documentation Platforms" sidebar on the left,
 * and a main panel on the right showing setup settings for the selected platform.
 */
class SProtoBoyConnectDocsDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProtoBoyConnectDocsDialog) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Opens the dialog as a modal window parented to ParentWidget's window. */
	static void Open(const TSharedPtr<SWidget>& ParentWidget);

private:
	TSharedRef<SWidget> MakePlatformEntry(EProtoBoyDocPlatform Platform, const FText& Label, const FName& IconName);

	TSharedRef<SWidget> MakeEmptyPanel();
	TSharedRef<SWidget> MakeObsidianPanel();

	void SelectPlatform(EProtoBoyDocPlatform Platform);

	EProtoBoyDocPlatform SelectedPlatform = EProtoBoyDocPlatform::None;
	TSharedPtr<SWidgetSwitcher> PanelSwitcher;
};
