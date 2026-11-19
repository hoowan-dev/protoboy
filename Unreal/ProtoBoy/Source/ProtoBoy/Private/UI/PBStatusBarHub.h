#pragma once

#include "CoreMinimal.h"

/**
 * The ProtoBoy hub: a button in the editor's bottom status bar, beside Revision Control, in every window that has a
 * status bar (the level editor, asset editors, ...). Its main job is the documentation health checks, read from
 * FProtoBoyDocHealthService:
 * - The button always reads "ProtoBoy"; an icon beside the name says where the docs stand: a throbber while checking,
 *   a warning when docs may need updating or the structure has problems, a check when healthy, a question mark when
 *   not checked yet. Its tooltip has the counts and the detail.
 * - Clicking it opens two menus side by side. On the left, the page options: the dashboard and each of its pages
 *   (SProtoBoyDashboard::GetPageDefinitions). To their right, the health checking: the status in words, Run Health
 *   Checks (the structure checks, then the check against revision control when that's enabled; Cancel while one
 *   runs), and the docs that may need updating (click to open one).
 *
 * Each editor app has its own status bar toolbar menu ("<StatusBarName>.ToolBar") with no shared parent, so the hub
 * adds itself to each one the first time its widget is generated (UToolMenus::OnPreGenerateWidget).
 */
namespace ProtoBoyStatusBarHub
{
	/** The hub's menus, so other code can extend them (UToolMenus::ExtendMenu): the page options on the left... */
	extern const FName MenuName;
	/** ...and the health status and checks to their right. */
	extern const FName HealthMenuName;

	/** Add the hub to the status bars. Call once UToolMenus is ready (FProtoBoyModule::RegisterMenus). */
	void Register();
	/** Remove it again (module shutdown). */
	void Unregister();
}
