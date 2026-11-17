#include "UI/Sourcing/SPBAssetSourcingPage.h"

#include "Async/Async.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "HAL/PlatformProcess.h"
#include "IImageWrapperModule.h"
#include "ImageCore.h"
#include "MCP/PBMCPClientConfig.h"
#include "Modules/ModuleManager.h"
#include "Sourcing/Fab/PBFabListings.h"
#include "Sourcing/PBAssetSearchRun.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "UI/Agents/SPBAgentPicker.h"
#include "UI/PBStyle.h"
#include "UI/PBWidgets.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "InputCoreTypes.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
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

		// Agent Log and Suggested Assets: only once an agent search has started.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 12.f, 0.f, 0.f)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return IsUnlocked.Get() && Run.IsValid() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				ProtoBoyWidgets::MakeSection(LOCTEXT("AgentLogHeader", "Agent Log"), MakeAgentLog())
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 12.f, 0.f, 0.f)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return IsUnlocked.Get() && Run.IsValid() && Run->Succeeded() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				ProtoBoyWidgets::MakeSection(LOCTEXT("SuggestionsHeader", "Suggested Assets"), MakeSuggestions())
			]
		]
	];
}

SProtoBoyAssetSourcingPage::~SProtoBoyAssetSourcingPage()
{
	if (PollHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PollHandle);
	}
	if (Run.IsValid())
	{
		Run->Cancel();
	}
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
			.Text(LOCTEXT("SearchDesc", "Describe the asset you need. Without a search context, the search opens as typed on every enabled source in your browser, where you pick the listings. With one, the agent first suggests the assets to look for, each with its own search."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		]

		// Query: a few lines tall and capped in width. Enter searches, Shift+Enter adds a line.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 8.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			.IsEnabled_Lambda([this]() { return CanEditSearch(); })

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
			.IsEnabled_Lambda([this]() { return CanEditSearch() && (Search.bUseDocsContext || Search.bUseProjectAssetsContext); })
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
			.IsEnabled_Lambda([this]() { return CanEditSearch(); })
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
			.IsEnabled_Lambda([this]() { return CanEditSearch(); })
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
			SNew(SBox)
			.IsEnabled_Lambda([this]() { return CanEditSearch(); })
			[
				MakeTypeFilters()
			]
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
			SNew(SBox)
			.IsEnabled_Lambda([this]() { return CanEditSearch(); })
			[
				MakePriceFilters()
			]
		]

		// Search
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.Padding(0.f, 12.f, 0.f, 0.f)
		[
			SNew(SButton)
			.Text_Lambda([this]() { return IsSearching() ? LOCTEXT("CancelSearch", "Cancel") : LOCTEXT("Search", "Search"); })
			.ToolTipText_Lambda([this]()
			{
				return IsSearching() ? LOCTEXT("CancelSearchTip", "Stop the search. Listings already loaded stay.")
					: Search.bUseDocsContext || Search.bUseProjectAssetsContext
					? LOCTEXT("SearchAgentTip", "Have the agent suggest assets from your description and the search context. Each suggestion has a search to open on the enabled sources.")
					: LOCTEXT("SearchTip", "Open this search on every enabled source, in your browser.");
			})
			.IsEnabled_Lambda([this]() { return IsSearching() || !Search.Query.TrimStartAndEnd().IsEmpty(); })
			.OnClicked_Lambda([this]()
			{
				if (IsSearching())
				{
					Run->Cancel();
				}
				else
				{
					OnSearch();
				}
				return FReply::Handled();
			})
		]

		// Status: a throbber while the agent runs, else the last search's result.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([this]() { return IsSearching() || !SearchStatus.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(SCircularThrobber)
				.Radius(7.f)
				.Visibility_Lambda([this]() { return IsSearching() ? EVisibility::Visible : EVisibility::Collapsed; })
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					if (!IsSearching())
					{
						return SearchStatus;
					}
					return Run->IsFinished()
						? LOCTEXT("LoadingListings", "Loading the listings from Fab...")
						: FText::Format(LOCTEXT("AgentSearching", "{0} is suggesting assets..."), FProtoBoyMCPClientConfig::GetClientName(Run->GetClient()));
				})
				.ColorAndOpacity_Lambda([this]() { return bSearchFailed && !IsSearching() ? FSlateColor(FStyleColors::Error) : FSlateColor::UseSubduedForeground(); })
				.AutoWrapText(true)
			]
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
	if (Search.MinPrice.IsSet() && Search.MaxPrice.IsSet() && Search.MinPrice.GetValue() > Search.MaxPrice.GetValue())
	{
		bSearchFailed = true;
		SearchStatus = LOCTEXT("BadPriceRange", "The minimum price is above the maximum.");
		return;
	}

	// The box allows several lines; sources get them as one line of search terms.
	FProtoBoyAssetSearch ToOpen = Search;
	ToOpen.Query = ToOpen.Query.Replace(TEXT("\r"), TEXT(" ")).Replace(TEXT("\n"), TEXT(" "));

	// With a search context, the agent suggests assets first; each one is opened from Suggested Assets.
	if (Search.bUseDocsContext || Search.bUseProjectAssetsContext)
	{
		StartAgentSearch(ToOpen);
		return;
	}

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

// --- Agent search ---------------------------------------------------------------------------------------------------

bool SProtoBoyAssetSourcingPage::IsSearching() const
{
	return Run.IsValid() && (!Run->IsFinished() || Run->IsLoadingPreviews());
}

void SProtoBoyAssetSourcingPage::StartAgentSearch(const FProtoBoyAssetSearch& ToRun)
{
	if (!SelectedAgent.IsSet())
	{
		bSearchFailed = true;
		SearchStatus = LOCTEXT("NoAgent", "Pick an agent to suggest assets, or untick the search context options to search as typed.");
		return;
	}

	const TSharedRef<FProtoBoyAssetSearchRun> NewRun = MakeShared<FProtoBoyAssetSearchRun>();
	FText Error;
	if (!NewRun->Start(SelectedAgent.GetValue(), ToRun, Error))
	{
		bSearchFailed = true;
		SearchStatus = Error;
		return;
	}

	Run = NewRun;
	bSearchFailed = false;
	SearchStatus = FText::GetEmpty();
	LogShown = 0;
	LogScroll->ClearChildren();
	SuggestionList->ClearChildren();
	SuggestionStatus = FText::GetEmpty();
	bSuggestionFailed = false;
	bShownFinish = false;
	CheckedRevision = -1;
	PreviewBrushes.Reset();
	DecodingPreviews.Reset();

	// The core ticker, not an active timer: the dashboard's other pages hide this one, and hidden widgets don't tick.
	if (PollHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PollHandle);
	}
	PollHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &SProtoBoyAssetSourcingPage::PollRun), 0.1f);
	PollRun(0.f);
}

bool SProtoBoyAssetSourcingPage::PollRun(float DeltaTime)
{
	if (!Run.IsValid())
	{
		PollHandle.Reset();
		return false;
	}
	Run->Tick();

	const TArray<FString>& Log = Run->GetLog();
	if (LogShown < Log.Num())
	{
		for (; LogShown < Log.Num(); ++LogShown)
		{
			LogScroll->AddSlot()
			.Padding(0.f, 1.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Log[LogShown]))
				.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
				.AutoWrapText(true)
			];
		}
		LogScroll->ScrollToEnd();
	}

	if (!Run->IsFinished())
	{
		return true;
	}
	if (!bShownFinish)
	{
		bShownFinish = true;
		if (Run->Succeeded())
		{
			ShowSuggestions();
			SearchStatus = FText::Format(LOCTEXT("AgentDone", "{0} suggested {1} {1}|plural(one=asset,other=assets). See Suggested Assets below."),
				FProtoBoyMCPClientConfig::GetClientName(Run->GetClient()), FText::AsNumber(Run->GetSuggestions().Num()));
		}
		else
		{
			bSearchFailed = !Run->WasCancelled();
			SearchStatus = Run->WasCancelled()
				? LOCTEXT("AgentCancelled", "Cancelled.")
				: LOCTEXT("AgentFailed", "The agent didn't suggest any assets. See the Agent Log for why.");
		}
	}

	// The tiles read the run's data directly; only the downloaded images need decoding into brushes.
	if (Run->GetRevision() != CheckedRevision)
	{
		CheckedRevision = Run->GetRevision();
		DecodePreviews();
	}
	if (Run->IsLoadingPreviews())
	{
		return true;
	}
	PollHandle.Reset();
	return false;
}

TSharedRef<SWidget> SProtoBoyAssetSourcingPage::MakeAgentLog()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
		.Padding(8.f)
		[
			// Fixed height and its own scroll bar, so a long run doesn't push Suggested Assets down the page.
			SNew(SBox)
			.HeightOverride(200.f)
			[
				SAssignNew(LogScroll, SScrollBox)
				.Orientation(Orient_Vertical)
				.ScrollBarAlwaysVisible(true)
			]
		];
}

TSharedRef<SWidget> SProtoBoyAssetSourcingPage::MakeSuggestions()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 6.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SuggestionsDesc", "What the agent suggests, from your description and the search context, with the Fab listings it found for each. Click a listing to open it in your browser. Check the price and license on its page."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(SuggestionList, SVerticalBox)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Visibility_Lambda([this]() { return SuggestionStatus.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
			.Text_Lambda([this]() { return SuggestionStatus; })
			.ColorAndOpacity_Lambda([this]() { return bSuggestionFailed ? FSlateColor(FStyleColors::Error) : FSlateColor::UseSubduedForeground(); })
			.AutoWrapText(true)
		];
}

void SProtoBoyAssetSourcingPage::ShowSuggestions()
{
	SuggestionList->ClearChildren();
	if (!Run.IsValid())
	{
		return;
	}
	for (int32 Index = 0; Index < Run->GetSuggestions().Num(); ++Index)
	{
		SuggestionList->AddSlot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			MakeSuggestion(Index)
		];
	}
}

const FProtoBoyAssetListing* SProtoBoyAssetSourcingPage::FindListing(int32 SuggestionIndex, int32 ListingIndex) const
{
	if (!Run.IsValid() || !Run->GetSuggestions().IsValidIndex(SuggestionIndex))
	{
		return nullptr;
	}
	const TArray<FProtoBoyAssetListing>& Listings = Run->GetSuggestions()[SuggestionIndex].Listings;
	return Listings.IsValidIndex(ListingIndex) ? &Listings[ListingIndex] : nullptr;
}

TSharedRef<SWidget> SProtoBoyAssetSourcingPage::MakeSuggestion(int32 SuggestionIndex)
{
	const FProtoBoyAssetSuggestion& Suggestion = Run->GetSuggestions()[SuggestionIndex];
	const FProtoBoyAssetSearch ToOpen = Suggestion.Search;

	TSharedRef<SVerticalBox> Card = SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Suggestion.Name))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
				.AutoWrapText(true)
			]

			// The search behind the suggestion, for more listings like these.
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("MoreLikeThis", "More Like This"))
				.ToolTipText(FText::Format(LOCTEXT("MoreLikeThisTip", "Search for \"{0}\" on every enabled source, in your browser, with your filters."), FText::FromString(ToOpen.Query)))
				.OnClicked_Lambda([this, ToOpen]()
				{
					TArray<FText> Errors;
					const int32 Opened = ProtoBoyAssetSourcing::OpenSearch(ToOpen, Errors);
					bSuggestionFailed = !Errors.IsEmpty();
					SuggestionStatus = bSuggestionFailed
						? FText::Join(FText::FromString(TEXT("\n")), Errors)
						: FText::Format(LOCTEXT("SuggestionOpened", "Opened the search \"{0}\" on {1} {1}|plural(one=source,other=sources) in your browser."), FText::FromString(ToOpen.Query), Opened);
					return FReply::Handled();
				})
			]
		];

	if (!Suggestion.DocTitle.IsEmpty())
	{
		Card->AddSlot()
		.AutoHeight()
		.Padding(0.f, 1.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("FromDoc", "From the Content doc \"{0}\""), FText::FromString(Suggestion.DocTitle)))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
		];
	}
	if (!Suggestion.Description.IsEmpty())
	{
		Card->AddSlot()
		.AutoHeight()
		.Padding(0.f, 3.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Suggestion.Description))
			.AutoWrapText(true)
		];
	}

	// Listing tiles, wrapping to the page's width.
	TSharedRef<SWrapBox> Tiles = SNew(SWrapBox)
		.UseAllottedSize(true)
		.InnerSlotPadding(FVector2D(8.f, 8.f));
	for (int32 ListingIndex = 0; ListingIndex < Suggestion.Listings.Num(); ++ListingIndex)
	{
		Tiles->AddSlot()
		[
			MakeListingTile(SuggestionIndex, ListingIndex)
		];
	}
	Card->AddSlot()
	.AutoHeight()
	.Padding(0.f, 8.f, 0.f, 0.f)
	[
		Tiles
	];

	// None found, or every one the agent gave turned out not to exist.
	Card->AddSlot()
	.AutoHeight()
	.Padding(0.f, 4.f, 0.f, 0.f)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("NoListings", "No listings on Fab for this within your filters. Try More Like This to search for it."))
		.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		.AutoWrapText(true)
		.Visibility_Lambda([this, SuggestionIndex]()
		{
			if (!Run.IsValid() || !Run->GetSuggestions().IsValidIndex(SuggestionIndex))
			{
				return EVisibility::Collapsed;
			}
			const bool bAnyShown = Run->GetSuggestions()[SuggestionIndex].Listings.ContainsByPredicate([](const FProtoBoyAssetListing& Listing) { return Listing.IsShown(); });
			return bAnyShown ? EVisibility::Collapsed : EVisibility::Visible;
		})
	];

	return SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		.Padding(8.f)
		[
			Card
		];
}

TSharedRef<SWidget> SProtoBoyAssetSourcingPage::MakeListingTile(int32 SuggestionIndex, int32 ListingIndex)
{
	constexpr float TileWidth = 240.f;
	constexpr float ImageHeight = 135.f;

	auto IsPreviewLoading = [this, SuggestionIndex, ListingIndex]()
	{
		const FProtoBoyAssetListing* Listing = FindListing(SuggestionIndex, ListingIndex);
		// Still "Loading" once nothing is loading: the loads were cancelled.
		if (!Listing || (!Run->IsLoadingPreviews() && Listing->Preview == FProtoBoyAssetListing::EPreview::Loading))
		{
			return false;
		}
		return Listing->Preview == FProtoBoyAssetListing::EPreview::Loading
			|| (Listing->Preview == FProtoBoyAssetListing::EPreview::Ready && !PreviewBrushes.Contains(Listing->Url));
	};
	auto GetBrush = [this, SuggestionIndex, ListingIndex]() -> const FSlateBrush*
	{
		const FProtoBoyAssetListing* Listing = FindListing(SuggestionIndex, ListingIndex);
		const TSharedPtr<FSlateDynamicImageBrush>* Brush = Listing ? PreviewBrushes.Find(Listing->Url) : nullptr;
		return Brush && Brush->IsValid() ? Brush->Get() : nullptr;
	};

	return SNew(SBox)
		.WidthOverride(TileWidth)
		.Visibility_Lambda([this, SuggestionIndex, ListingIndex]()
		{
			const FProtoBoyAssetListing* Listing = FindListing(SuggestionIndex, ListingIndex);
			return Listing && Listing->IsShown() ? EVisibility::Visible : EVisibility::Collapsed;
		})
		[
			SNew(SButton)
			.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
			.ContentPadding(4.f)
			.ToolTipText_Lambda([this, SuggestionIndex, ListingIndex]()
			{
				const FProtoBoyAssetListing* Listing = FindListing(SuggestionIndex, ListingIndex);
				return Listing ? FText::Format(LOCTEXT("ListingTip", "Open {0} in your browser."), FText::FromString(Listing->Url)) : FText::GetEmpty();
			})
			.OnClicked_Lambda([this, SuggestionIndex, ListingIndex]()
			{
				if (const FProtoBoyAssetListing* Listing = FindListing(SuggestionIndex, ListingIndex))
				{
					FString Error;
					FPlatformProcess::LaunchURL(*Listing->Url, nullptr, &Error);
					bSuggestionFailed = !Error.IsEmpty();
					SuggestionStatus = bSuggestionFailed
						? FText::Format(LOCTEXT("ListingOpenFailed", "Couldn't open the browser: {0}"), FText::FromString(Error))
						: FText::GetEmpty();
				}
				return FReply::Handled();
			})
			[
				SNew(SVerticalBox)

				// Preview: the image, a throbber while it loads, or "No preview".
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.WidthOverride(TileWidth)
					.HeightOverride(ImageHeight)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
						.Padding(0.f)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(SOverlay)

							+ SOverlay::Slot()
							[
								SNew(SScaleBox)
								.Stretch(EStretch::ScaleToFill)
								.Visibility_Lambda([GetBrush]() { return GetBrush() ? EVisibility::Visible : EVisibility::Collapsed; })
								[
									SNew(SImage)
									.Image_Lambda(GetBrush)
								]
							]

							+ SOverlay::Slot()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SNew(SCircularThrobber)
								.Radius(10.f)
								.Visibility_Lambda([IsPreviewLoading]() { return IsPreviewLoading() ? EVisibility::Visible : EVisibility::Collapsed; })
							]

							+ SOverlay::Slot()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("NoPreview", "No preview"))
								.ColorAndOpacity(FSlateColor::UseSubduedForeground())
								.Visibility_Lambda([GetBrush, IsPreviewLoading]() { return !GetBrush() && !IsPreviewLoading() ? EVisibility::Visible : EVisibility::Collapsed; })
							]
						]
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 6.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text_Lambda([this, SuggestionIndex, ListingIndex]()
					{
						const FProtoBoyAssetListing* Listing = FindListing(SuggestionIndex, ListingIndex);
						return Listing ? FText::FromString(Listing->Title.IsEmpty() ? Listing->Url : Listing->Title) : FText::GetEmpty();
					})
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
					.AutoWrapText(true)
				]

				// Price: "Free", "From $8.39", with the price before a sale; "Price on Fab" until the page is read.
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 3.f, 0.f, 0.f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text_Lambda([this, SuggestionIndex, ListingIndex]()
						{
							const FProtoBoyAssetListing* Listing = FindListing(SuggestionIndex, ListingIndex);
							if (!Listing || !Listing->Price.IsSet())
							{
								return Listing && Listing->Preview == FProtoBoyAssetListing::EPreview::Loading && Run->IsLoadingPreviews()
									? FText::GetEmpty()
									: LOCTEXT("PriceUnknown", "Price on Fab");
							}
							const FString Price = ProtoBoyFab::FormatPrice(Listing->Price.GetValue(), Listing->Currency);
							return Listing->Price.GetValue() < 0.005
								? FText::FromString(Price)
								: FText::Format(LOCTEXT("PriceFrom", "From {0}"), FText::FromString(Price));
						})
						.ToolTipText(LOCTEXT("PriceTip", "The cheapest license (usually Personal). Other licenses, like Professional, cost more. Check on the listing's page."))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
						.ColorAndOpacity_Lambda([this, SuggestionIndex, ListingIndex]()
						{
							const FProtoBoyAssetListing* Listing = FindListing(SuggestionIndex, ListingIndex);
							return Listing && Listing->Price.IsSet() && Listing->Price.GetValue() < 0.005 ? FSlateColor(FStyleColors::Success) : FSlateColor::UseForeground();
						})
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(6.f, 0.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Text_Lambda([this, SuggestionIndex, ListingIndex]()
						{
							const FProtoBoyAssetListing* Listing = FindListing(SuggestionIndex, ListingIndex);
							return Listing && Listing->ListPrice.IsSet()
								? FText::Format(LOCTEXT("PriceWas", "was {0}"), FText::FromString(ProtoBoyFab::FormatPrice(Listing->ListPrice.GetValue(), Listing->Currency)))
								: FText::GetEmpty();
						})
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 2.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text_Lambda([this, SuggestionIndex, ListingIndex]()
					{
						const FProtoBoyAssetListing* Listing = FindListing(SuggestionIndex, ListingIndex);
						return Listing ? FText::FromString(Listing->Note) : FText::GetEmpty();
					})
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.AutoWrapText(true)
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 2.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text_Lambda([this, SuggestionIndex, ListingIndex]()
					{
						const FProtoBoyAssetListing* Listing = FindListing(SuggestionIndex, ListingIndex);
						return Listing ? FText::FromString(Listing->SourceName) : FText::GetEmpty();
					})
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]
		];
}

void SProtoBoyAssetSourcingPage::DecodePreviews()
{
	if (!Run.IsValid())
	{
		return;
	}
	// Loaded on the game thread; decoding then runs on a worker.
	IImageWrapperModule& ImageWrapper = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	const TWeakPtr<SProtoBoyAssetSourcingPage> WeakThis = StaticCastSharedRef<SProtoBoyAssetSourcingPage>(AsShared());

	for (const FProtoBoyAssetSuggestion& Suggestion : Run->GetSuggestions())
	{
		for (const FProtoBoyAssetListing& Listing : Suggestion.Listings)
		{
			if (Listing.Preview != FProtoBoyAssetListing::EPreview::Ready || PreviewBrushes.Contains(Listing.Url) || DecodingPreviews.Contains(Listing.Url))
			{
				continue;
			}
			DecodingPreviews.Add(Listing.Url);
			Async(EAsyncExecution::ThreadPool, [&ImageWrapper, WeakThis, Url = Listing.Url, Data = Listing.ImageData]()
			{
				// A small thumbnail: tiles are 240 wide, so 2x covers high-DPI screens.
				constexpr int32 MaxWidth = 480;
				TArray<uint8> Pixels;
				FIntPoint Size(0, 0);
				FImage Decoded;
				if (ImageWrapper.DecompressImage(Data.GetData(), Data.Num(), Decoded) && Decoded.SizeX > 0 && Decoded.SizeY > 0)
				{
					Size.X = FMath::Min(MaxWidth, Decoded.SizeX);
					Size.Y = FMath::Max(1, FMath::RoundToInt(static_cast<float>(Size.X) * Decoded.SizeY / Decoded.SizeX));
					FImage Thumbnail;
					Decoded.ResizeTo(Thumbnail, Size.X, Size.Y, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
					Pixels.Append(Thumbnail.RawData.GetData(), static_cast<int32>(Thumbnail.RawData.Num()));
				}
				AsyncTask(ENamedThreads::GameThread, [WeakThis, Url, Pixels = MoveTemp(Pixels), Size]()
				{
					const TSharedPtr<SProtoBoyAssetSourcingPage> This = WeakThis.Pin();
					if (!This.IsValid())
					{
						return;
					}
					This->DecodingPreviews.Remove(Url);
					TSharedPtr<FSlateDynamicImageBrush> Brush;
					if (!Pixels.IsEmpty())
					{
						Brush = FSlateDynamicImageBrush::CreateWithImageData(FName(*FString::Printf(TEXT("ProtoBoyListing_%s"), *Url)), FVector2D(Size.X, Size.Y), Pixels);
					}
					This->PreviewBrushes.Add(Url, Brush);
				});
			});
		}
	}
}

#undef LOCTEXT_NAMESPACE
