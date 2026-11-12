#pragma once

#include "Architecture/PBPillarDocs.h"
#include "Widgets/SCompoundWidget.h"

class SWindow;

/** "Generate Pillar Docs" window: lists every planned change and writes them only when the user clicks Apply. */
class SProtoBoyPillarDocsPreview : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProtoBoyPillarDocsPreview) {}
		SLATE_ARGUMENT(TWeakPtr<SWindow>, ParentWindow)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Plans the changes and opens the preview as a modal window. */
	static void Open(const TSharedPtr<SWidget>& ParentWidget);

private:
	TSharedRef<SWidget> MakeChangeRow(const FProtoBoyPillarDocChange& Change);
	FReply OnApplyClicked();

	TArray<FProtoBoyPillarDocChange> Changes;
	TWeakPtr<SWindow> ParentWindow;
	FText ApplyError;
	FText DocsLocation;
};
