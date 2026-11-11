#pragma once

#include "Styling/SlateStyle.h"

/** Slate style set for ProtoBoy's own icons (loaded from the plugin's Resources folder). */
class FProtoBoyStyle
{
public:
	static void Initialize();
	static void Shutdown();
	static const ISlateStyle& Get();

private:
	static TSharedPtr<FSlateStyleSet> StyleSet;
};
