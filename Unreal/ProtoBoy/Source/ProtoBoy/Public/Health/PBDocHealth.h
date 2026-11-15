#pragma once

#include "CoreMinimal.h"

/**
 * Documentation health checks (optional, Setup > Documentation Health Checks): with an agent, find files changed in
 * revision control that may be related to existing notes, so the team can see which docs may need updating. Needs the
 * editor to be connected to revision control (Perforce, Git, Unity Version Control, ...).
 *
 * Stub: the setting and the connection check exist; running the checks does nothing yet.
 */
namespace ProtoBoyDocHealth
{
	/** True if the editor is connected to a revision control provider that's available. OutStatus says which, or why not. */
	PROTOBOY_API bool IsRevisionControlConnected(FText& OutStatus);

	/** The team setting (UProtoBoySettings::bEnableDocHealthChecks), and only while revision control is connected. */
	PROTOBOY_API bool IsEnabled();

	/** Open the editor's Revision Control login window, keeping the current provider. */
	PROTOBOY_API void OpenRevisionControlLogin();

	/**
	 * TODO: Ask an agent which docs may need updating: list files changed in revision control since each doc on the map
	 * was last edited (history from the connected provider), match them to the docs whose Tech and Content sections name
	 * them (paths, classes, folders), and report the likely-stale docs with the changes behind each. Does nothing yet.
	 */
	PROTOBOY_API void RunHealthChecks();
}
