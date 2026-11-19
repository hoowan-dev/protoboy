#pragma once

#include "Widgets/SCompoundWidget.h"

/**
 * The dashboard's Health page: runs the documentation health checks (ProtoBoyDocHealth::RunHealthChecks, a stub) and
 * will list the docs that may need updating.
 * Locked (only LockMessage shows) while IsUnlocked is false: until Enable documentation health checks is ticked in
 * Setup > Documentation Health Checks (SProtoBoyDocHealthPanel) and revision control is connected
 * (ProtoBoyDocHealth::IsEnabled).
 */
class SProtoBoyHealthPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProtoBoyHealthPage)
		: _IsUnlocked(true)
	{}
		/** False until health checks are enabled; the page then shows only LockMessage. */
		SLATE_ATTRIBUTE(bool, IsUnlocked)
		/** Why the page is locked, and where to fix it (the dashboard's setup warning). */
		SLATE_NAMED_SLOT(FArguments, LockMessage)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** What the checks do, the revision control status and the Run Health Checks button. */
	TSharedRef<SWidget> MakeChecks();

	// TODO: a Results section listing the likely-stale docs, each with the changed files behind it and a button to open
	// the doc, once RunHealthChecks reports them.

	TAttribute<bool> IsUnlocked;
};
