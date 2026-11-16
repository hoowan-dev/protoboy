#pragma once

#include "CoreMinimal.h"

class SWidget;

/** Small widget builders shared by the dashboard's pages, so they look the same. */
namespace ProtoBoyWidgets
{
	/** Wraps Body in a collapsible, titled card styled like a Details panel category. Use for every section in a page. */
	TSharedRef<SWidget> MakeSection(const FText& Title, const TSharedRef<SWidget>& Body);
}
