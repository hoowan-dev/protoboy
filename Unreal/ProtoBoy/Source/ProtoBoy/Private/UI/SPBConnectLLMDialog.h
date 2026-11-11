#pragma once

#include "Widgets/SCompoundWidget.h"

class SWidgetSwitcher;

/** LLM providers ProtoBoy's MCP integration can connect to. Values double as panel switcher indices. */
enum class EProtoBoyLLMProvider : uint8
{
	None,
	Claude,
	Gemini,
};

/**
 * "Connect LLM Provider" window content (opened from the dashboard's MCP section):
 * an "LLM Providers" sidebar on the left, and a main panel on the right showing setup for the selected provider.
 */
class SProtoBoyConnectLLMDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProtoBoyConnectLLMDialog) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Opens the dialog as a modal window parented to ParentWidget's window. */
	static void Open(const TSharedPtr<SWidget>& ParentWidget);

private:
	TSharedRef<SWidget> MakeProviderEntry(EProtoBoyLLMProvider Provider, const FText& Label, const FName& IconName);

	TSharedRef<SWidget> MakeEmptyPanel();

	/** TODO: Replace with a real setup panel per provider (MCP server config, auth, etc.). */
	TSharedRef<SWidget> MakeProviderPanel(const FText& ProviderName);

	void SelectProvider(EProtoBoyLLMProvider Provider);

	EProtoBoyLLMProvider SelectedProvider = EProtoBoyLLMProvider::None;
	TSharedPtr<SWidgetSwitcher> PanelSwitcher;
};
