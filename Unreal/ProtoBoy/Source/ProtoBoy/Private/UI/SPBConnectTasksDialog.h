#pragma once

#include "Tasks/PBTasksConnection.h"
#include "Widgets/SCompoundWidget.h"

class SWidgetSwitcher;

/**
 * "Connect Task Board" window content (opened from the dashboard's Tasks section):
 * a "Board Providers" sidebar on the left (Kanban-style boards: Trello for now; Jira, Basecamp, ... later),
 * and a main panel on the right showing setup for the selected provider.
 */
class SProtoBoyConnectTasksDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProtoBoyConnectTasksDialog) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Opens the dialog as a modal window parented to ParentWidget's window. */
	static void Open(const TSharedPtr<SWidget>& ParentWidget);

private:
	/** EProtoBoyTaskProvider values double as PanelSwitcher slot indices. */
	using EProvider = EProtoBoyTaskProvider;

	TSharedRef<SWidget> MakeProviderEntry(EProvider Provider, const FText& Label, const FName& IconName);

	TSharedRef<SWidget> MakeEmptyPanel();

	void SelectProvider(EProvider Provider);

	EProvider SelectedProvider = EProvider::None;
	TSharedPtr<SWidgetSwitcher> PanelSwitcher;
};
