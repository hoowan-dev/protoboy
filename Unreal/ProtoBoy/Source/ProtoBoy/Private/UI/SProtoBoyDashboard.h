#pragma once

#include "Widgets/SCompoundWidget.h"

/** Placeholder dashboard: daemon status, PVI, health issues, docs actions. */
class SProtoBoyDashboard : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProtoBoyDashboard) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};
