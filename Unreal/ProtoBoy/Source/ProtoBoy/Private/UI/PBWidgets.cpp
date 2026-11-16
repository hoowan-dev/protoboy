#include "UI/PBWidgets.h"

#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SExpandableArea.h"

namespace ProtoBoyWidgets
{
	TSharedRef<SWidget> MakeSection(const FText& Title, const TSharedRef<SWidget>& Body)
	{
		// Collapsible card styled like a Details panel category: tinted header bar, panel-colored body.
		return SNew(SExpandableArea)
			.AreaTitle(Title)
			.AreaTitleFont(FCoreStyle::GetDefaultFontStyle("Bold", 12))
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Header"))
			.BodyBorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
			.HeaderPadding(FMargin(8.f, 6.f))
			.Padding(FMargin(12.f, 10.f))
			.BodyContent()
			[
				Body
			];
	}
}
