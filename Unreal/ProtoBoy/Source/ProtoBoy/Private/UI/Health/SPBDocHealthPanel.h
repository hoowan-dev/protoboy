#pragma once

#include "Widgets/SCompoundWidget.h"

/**
 * Setup > Documentation Health Checks (optional) on the dashboard: what health checks do, the team-wide checkbox
 * (UProtoBoySettings::bEnableDocHealthChecks, only enabled while the editor is connected to revision control), the
 * revision control status, and how to connect. The checks themselves are a stub (ProtoBoyDocHealth::RunHealthChecks).
 */
class SProtoBoyDocHealthPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProtoBoyDocHealthPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};
