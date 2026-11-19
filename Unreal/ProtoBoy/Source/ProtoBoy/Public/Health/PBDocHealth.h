#pragma once

#include "CoreMinimal.h"

/**
 * Documentation health checks (optional, Setup > Documentation Health Checks): with an agent, find files changed in
 * revision control that may be related to existing notes, so the team can see which docs may need updating. Needs the
 * editor to be connected to revision control (Perforce, Git, Unity Version Control, ...).
 *
 * This is the setting and the connection check. A check itself is an FProtoBoyDocHealthRun (Health/PBDocHealthRun.h),
 * started from the dashboard's Health page: ProtoBoyDocHealthChanges lists what changed since each doc was edited, an
 * agent judges which docs that outdates, and the result is kept as a ProtoBoyDocHealthReport.
 */
namespace ProtoBoyDocHealth
{
	/** True if the editor is connected to a revision control provider that's available. OutStatus says which, or why not. */
	PROTOBOY_API bool IsRevisionControlConnected(FText& OutStatus);

	/** The team setting (UProtoBoySettings::bEnableDocHealthChecks), and only while revision control is connected. */
	PROTOBOY_API bool IsEnabled();

	/** Open the editor's Revision Control login window, keeping the current provider. */
	PROTOBOY_API void OpenRevisionControlLogin();
}
