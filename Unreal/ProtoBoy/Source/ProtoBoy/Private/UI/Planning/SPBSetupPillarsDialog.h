#pragma once

#include "Widgets/SCompoundWidget.h"

class SWindow;

/** "Setup Pillars" window, opened from Planning > Pillars. Blank for now. */
class SProtoBoySetupPillarsDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProtoBoySetupPillarsDialog) {}
		SLATE_ARGUMENT(TWeakPtr<SWindow>, ParentWindow)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Opens the window as a modal over ParentWidget's window. */
	static void Open(const TSharedPtr<SWidget>& ParentWidget);

private:
	TWeakPtr<SWindow> ParentWindow;
};
