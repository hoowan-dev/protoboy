#pragma once

#include "CoreMinimal.h"

/**
 * The ProtoBoy hub: a "ProtoBoy" button in the editor's bottom status bar, beside Revision Control, in every window
 * that has a status bar (the level editor, asset editors, ...). Its menu opens the dashboard, or the dashboard on one
 * of its pages (SProtoBoyDashboard::GetPageDefinitions).
 *
 * Each editor app has its own status bar toolbar menu ("<StatusBarName>.ToolBar") with no shared parent, so the hub
 * adds itself to each one the first time its widget is generated (UToolMenus::OnPreGenerateWidget).
 *
 * TODO: show status on the button (setup not finished, docs that may need updating from the last health report).
 */
namespace ProtoBoyStatusBarHub
{
	/** The hub's menu, so other code can extend it (UToolMenus::ExtendMenu). */
	extern const FName MenuName;

	/** Add the hub to the status bars. Call once UToolMenus is ready (FProtoBoyModule::RegisterMenus). */
	void Register();
	/** Remove it again (module shutdown). */
	void Unregister();
}
