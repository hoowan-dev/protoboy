#include "UI/Sourcing/SPBAssetSourcingPage.h"

#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "UI/Agents/SPBAgentPicker.h"
#include "UI/PBStyle.h"
#include "UI/PBWidgets.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "InputCoreTypes.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ProtoBoyAssetSourcingPage"

void SProtoBoyAssetSourcingPage::Construct(const FArguments& InArgs)
{
	IsUnlocked = InArgs._IsUnlocked;
	SelectedAgent = SProtoBoyAgentPicker::GetDefaultAgent();

	auto WhenUnlocked = [this]() { return IsUnlocked.Get() ? EVisibility::Visible : EVisibility::Collapsed; };

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return IsUnlocked.Get() ? EVisibility::Collapsed : EVisibility::Visible; })
			[
				InArgs._LockMessage.Widget
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.Visibility_Lambda(WhenUnlocked)
			[
				ProtoBoyWidgets::MakeSection(LOCTEXT("SourcesHeader", "Sources"), MakeSources())
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 12.f, 0.f, 0.f)
		[
			SNew(SBox)
			.Visibility_Lambda(WhenUnlocked)
			[
				ProtoBoyWidgets::MakeSection(LOCTEXT("SearchHeader", "Search For Assets"), MakeSearch())
			]
		]
	];
}

TSharedRef<SWidget> SProtoBoyAssetSourcingPage::MakeSources()
{
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);

	List->AddSlot()
	.AutoHeight()
	.Padding(0.f, 0.f, 0.f, 6.f)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("SourcesDesc", "Where agents and searches look for assets. Turn sources on or off in Setup > Asset Sourcing."))
		.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		.AutoWrapText(true)
	];

	for (const FProtoBoyAssetSource& Source : ProtoBoyAssetSourcing::GetSources())
	{
		const TFunction<bool()> IsEnabled = Source.IsEnabled;

		List->AddSlot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
			.Padding(8.f)
			.ColorAndOpacity_Lambda([IsEnabled]() { return IsEnabled() ? FLinearColor::White : FLinearColor(1.f, 1.f, 1.f, 0.5f); })
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Top)
				.Padding(0.f, 0.f, 8.f, 0.f)
				[
					SNew(SImage)
					.Image(FProtoBoyStyle::Get().GetBrush(Source.Icon))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(Source.Name)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 2.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Text(Source.Description)
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.AutoWrapText(true)
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Top)
				.Padding(8.f, 0.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text_Lambda([IsEnabled]() { return IsEnabled() ? LOCTEXT("SourceEnabled", "Enabled") : LOCTEXT("SourceOff", "Off"); })
					.ColorAndOpacity_Lambda([IsEnabled]() { return IsEnabled() ? FSlateColor(FStyleColors::Success) : FSlateColor::UseSubduedForeground(); })
				]
			]
		];
	}

	return List;
}

TSharedRef<SWidget> SProtoBoyAssetSourcingPage::MakeSearch()
{
	auto Label = [](const FText& Text)
	{
		return SNew(STextBlock)
			.Text(Text)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10));
	};

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SearchDesc", "Describe the asset you need. The search opens on every enabled source in your browser, where you pick the listings."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		]

		// Query: a few lines tall and capped in width. Enter searches, Shift+Enter adds a line.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 8.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.MaxWidth(600.f)
			[
				SNew(SBox)
				.MinDesiredHeight(72.f)
				[
					SNew(SMultiLineEditableTextBox)
					.HintText(LOCTEXT("QueryHint", "Describe what to search for, e.g. \"stylized pine trees for a cozy autumn forest\""))
					.AutoWrapText(true)
					.Text_Lambda([this]() { return FText::FromString(Search.Query); })
					.OnTextChanged_Lambda([this](const FText& Text) { Search.Query = Text.ToString(); })
					.OnKeyDownHandler_Lambda([this](const FGeometry&, const FKeyEvent& KeyEvent)
					{
						if (KeyEvent.GetKey() == EKeys::Enter && !KeyEvent.IsShiftDown())
						{
							if (!Search.Query.TrimStartAndEnd().IsEmpty())
							{
								OnSearch();
							}
							return FReply::Handled();
						}
						return FReply::Unhandled();
					})
				]
			]
		]

		// Agent: only used for the search context below, so disabled while neither box is ticked.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 10.f, 0.f, 4.f)
		[
			Label(LOCTEXT("AgentLabel", "Agent"))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.IsEnabled_Lambda([this]() { return Search.bUseDocsContext || Search.bUseProjectAssetsContext; })
			.ToolTipText(LOCTEXT("AgentTip", "Turns your description and the search context into search terms for each source. Only used when a search context option below is ticked."))
			[
				SNew(SProtoBoyAgentPicker)
				.SelectedAgent_Lambda([this]() { return SelectedAgent; })
				.OnAgentPicked_Lambda([this](EProtoBoyMCPClient Agent) { SelectedAgent = Agent; })
			]
		]

		// Search context
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 10.f, 0.f, 0.f)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([this]() { return Search.bUseDocsContext ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { Search.bUseDocsContext = State == ECheckBoxState::Checked; })
			.ToolTipText(LOCTEXT("DocsContextTip", "Refine the search with what your ProtoBoy docs say about the game and its art direction (Content docs' Style, What to Look For and What to Avoid), including the images in them."))
			[
				SNew(STextBlock).Text(LOCTEXT("DocsContext", "Use this project's ProtoBoy Docs as search context (including images)"))
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([this]() { return Search.bUseProjectAssetsContext ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { Search.bUseProjectAssetsContext = State == ECheckBoxState::Checked; })
			.ToolTipText(LOCTEXT("AssetsContextTip", "Refine the search with the assets already in this project, so results match what's there and don't duplicate it."))
			[
				SNew(STextBlock).Text(LOCTEXT("AssetsContext", "Use this project's assets as search context"))
			]
		]

		// Asset types
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 10.f, 0.f, 4.f)
		[
			Label(LOCTEXT("TypesLabel", "Asset Types"))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			MakeTypeFilters()
		]

		// Price
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 10.f, 0.f, 4.f)
		[
			Label(LOCTEXT("PriceLabel", "Price"))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			MakePriceFilters()
		]

		// Search
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.Padding(0.f, 12.f, 0.f, 0.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("Search", "Search"))
			.ToolTipText(LOCTEXT("SearchTip", "Open this search on every enabled source, in your browser."))
			.IsEnabled_Lambda([this]() { return !Search.Query.TrimStartAndEnd().IsEmpty(); })
			.OnClicked_Lambda([this]() { OnSearch(); return FReply::Handled(); })
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Visibility_Lambda([this]() { return SearchStatus.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
			.Text_Lambda([this]() { return SearchStatus; })
			.ColorAndOpacity_Lambda([this]() { return bSearchFailed ? FSlateColor(FStyleColors::Error) : FSlateColor::UseSubduedForeground(); })
			.AutoWrapText(true)
		];
}

TSharedRef<SWidget> SProtoBoyAssetSourcingPage::MakeTypeFilters()
{
	// One row per group; each row wraps on narrow panels. None ticked searches every type.
	const TArray<TArray<EProtoBoyAssetType>> Rows = {
		{ EProtoBoyAssetType::Model3D },
		{ EProtoBoyAssetType::MaterialTexture, EProtoBoyAssetType::SpriteFlipbook, EProtoBoyAssetType::Decal, EProtoBoyAssetType::Brush, EProtoBoyAssetType::HDRI },
		{ EProtoBoyAssetType::Animation, EProtoBoyAssetType::Audio, EProtoBoyAssetType::VFX, EProtoBoyAssetType::UI },
		{ EProtoBoyAssetType::GameSystem, EProtoBoyAssetType::GameTemplate, EProtoBoyAssetType::ToolPlugin, EProtoBoyAssetType::Tutorial },
	};

	TSharedRef<SVerticalBox> Types = SNew(SVerticalBox);
	for (const TArray<EProtoBoyAssetType>& Row : Rows)
	{
		TSharedRef<SWrapBox> RowBox = SNew(SWrapBox)
			.UseAllottedSize(true)
			.InnerSlotPadding(FVector2D(12.f, 4.f));
		Types->AddSlot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			RowBox
		];

		for (const EProtoBoyAssetType Type : Row)
		{
			RowBox->AddSlot()
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this, Type]() { return Search.Types.Contains(Type) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this, Type](ECheckBoxState State)
				{
					if (State == ECheckBoxState::Checked)
					{
						Search.Types.AddUnique(Type);
					}
					else
					{
						Search.Types.Remove(Type);
					}
				})
				[
					SNew(STextBlock).Text(ProtoBoyAssetSourcing::GetAssetTypeName(Type))
				]
			];
		}
	}

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			Types
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Visibility_Lambda([this]() { return Search.Types.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
			.Text(LOCTEXT("AllTypes", "None ticked: every type."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
}

TSharedRef<SWidget> SProtoBoyAssetSourcingPage::MakePriceFilters()
{
	// A price box: empty or 0 is no limit.
	auto MakePriceBox = [this](TOptional<float> FProtoBoyAssetSearch::* Field)
	{
		return SNew(SBox)
			.WidthOverride(90.f)
			[
				SNew(SNumericEntryBox<float>)
				.AllowSpin(false)
				.MinValue(0.f)
				.MaxFractionalDigits(2)
				.UndeterminedString(LOCTEXT("PriceAny", "Any"))
				.IsEnabled_Lambda([this]() { return !Search.bFreeOnly; })
				.Value_Lambda([this, Field]() { return Search.*Field; })
				.OnValueCommitted_Lambda([this, Field](float Value, ETextCommit::Type)
				{
					Search.*Field = Value > 0.f ? TOptional<float>(Value) : TOptional<float>();
				})
			];
	};

	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([this]() { return Search.bFreeOnly ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { Search.bFreeOnly = State == ECheckBoxState::Checked; })
			[
				SNew(STextBlock).Text(LOCTEXT("FreeOnly", "Free only"))
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(16.f, 0.f, 6.f, 0.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PriceFrom", "From $"))
			.IsEnabled_Lambda([this]() { return !Search.bFreeOnly; })
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			MakePriceBox(&FProtoBoyAssetSearch::MinPrice)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(8.f, 0.f, 6.f, 0.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PriceTo", "to $"))
			.IsEnabled_Lambda([this]() { return !Search.bFreeOnly; })
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			MakePriceBox(&FProtoBoyAssetSearch::MaxPrice)
		];
}

void SProtoBoyAssetSourcingPage::OnSearch()
{
	// TODO: when bUseDocsContext or bUseProjectAssetsContext is set, run SelectedAgent (FProtoBoyAgentRun) first to turn the
	// query into better search terms per source: from the docs, the Content docs' Style / What to Look For / What to
	// Avoid and the images the docs embed; from the project, an asset registry summary of /Game (as doc generation
	// does). Today the query is opened as typed.

	if (Search.MinPrice.IsSet() && Search.MaxPrice.IsSet() && Search.MinPrice.GetValue() > Search.MaxPrice.GetValue())
	{
		bSearchFailed = true;
		SearchStatus = LOCTEXT("BadPriceRange", "The minimum price is above the maximum.");
		return;
	}

	// The box allows several lines; sources get them as one line of search terms.
	FProtoBoyAssetSearch ToOpen = Search;
	ToOpen.Query = ToOpen.Query.Replace(TEXT("\r"), TEXT(" ")).Replace(TEXT("\n"), TEXT(" "));

	TArray<FText> Errors;
	const int32 Opened = ProtoBoyAssetSourcing::OpenSearch(ToOpen, Errors);
	bSearchFailed = !Errors.IsEmpty();
	if (bSearchFailed)
	{
		SearchStatus = FText::Join(FText::FromString(TEXT("\n")), Errors);
	}
	else
	{
		SearchStatus = FText::Format(LOCTEXT("SearchOpened", "Opened the search on {0} {0}|plural(one=source,other=sources) in your browser."), Opened);
	}
}

#undef LOCTEXT_NAMESPACE
