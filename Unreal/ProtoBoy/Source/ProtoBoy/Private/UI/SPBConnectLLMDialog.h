#pragma once

#include "Widgets/SCompoundWidget.h"

class SWidgetSwitcher;
struct FSlateBrush;

/**
 * "Connect LLM Provider" window content (opened from the dashboard's MCP section).
 * Sidebar on the left: the MCP Server page, then one page per LLM provider. The main panel shows the selected page.
 */
class SProtoBoyConnectLLMDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProtoBoyConnectLLMDialog) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Opens the dialog as a modal window parented to ParentWidget's window. */
	static void Open(const TSharedPtr<SWidget>& ParentWidget);

	/** Keeps the MCP server answering while this modal window stalls the engine's core ticker. */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	/** Sidebar pages. Values double as PanelSwitcher slot indices. */
	enum class EPage : uint8
	{
		Server,
		Claude,
		Gemini,
	};

	TSharedRef<SWidget> MakePageEntry(EPage Page, const FText& Label, const FSlateBrush* Icon);
	TSharedRef<SWidget> MakeSidebarHeader(const FText& Text);

	void SelectPage(EPage Page);

	EPage SelectedPage = EPage::Server;
	TSharedPtr<SWidgetSwitcher> PanelSwitcher;
};
