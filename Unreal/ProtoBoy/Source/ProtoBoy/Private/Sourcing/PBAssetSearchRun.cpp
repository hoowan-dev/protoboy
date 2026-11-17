#include "Sourcing/PBAssetSearchRun.h"

#include "Agents/PBAgentPromptContext.h"
#include "Docs/PBDocLayers.h"
#include "Docs/PBDocMap.h"
#include "Dom/JsonObject.h"
#include "MCP/PBMCPClientConfig.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Net/PBHttpClient.h"
#include "PBSettings.h"
#include "PBSubsystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sourcing/Fab/PBFabListings.h"

#define LOCTEXT_NAMESPACE "ProtoBoyAssetSearch"

namespace ProtoBoyAssetSearch
{
	/** Per doc, so a big doc can't crowd out the others. */
	constexpr int32 MaxDocChars = 8000;
	/** More than this is ignored: the agent is asked for far fewer. */
	constexpr int32 MaxSuggestions = 20;
	/** The agent is asked for 1-3. */
	constexpr int32 MaxListingsPerAsset = 4;

	/** "Game doc > Combat (Design)": where a doc sits on the map, above it. */
	FString DescribeAncestors(const FProtoBoyDocMap& Map, const FProtoBoyDocNode& Doc)
	{
		TArray<FString> Chain;
		const FProtoBoyDocNode* Parent = Map.GetParent(Doc.Id);
		// The map has no cycles, but don't trust that blindly.
		for (int32 Depth = 0; Parent && Depth < 32; ++Depth)
		{
			Chain.Insert(FString::Printf(TEXT("\"%s\" (%s)"), *Parent->Title, *ProtoBoyDocLayers::GetDisplayName(Parent->Layer).ToString()), 0);
			Parent = Map.GetParent(Parent->Id);
		}
		return Chain.IsEmpty() ? FString(TEXT("the top of the map")) : FString::Join(Chain, TEXT(" > "));
	}

	/** The Game doc and every Content doc, for the style and the asset needs. */
	FString DescribeDocs()
	{
		const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
		if (!Subsystem)
		{
			return TEXT("_The docs aren't available._\n\n");
		}
		const FProtoBoyDocMap& Map = Subsystem->GetDocMap();

		FString Out = TEXT("## The project's docs\n\n");
		int32 NumContent = 0;
		for (const FProtoBoyDocNode& Doc : Map.GetNodes())
		{
			if (Doc.Layer == EProtoBoyDocLayer::Game)
			{
				Out += FString::Printf(TEXT("### Game doc \"%s\"\n\n%s\n"), *Doc.Title, *ProtoBoyAgentPrompt::ReadDoc(Doc, MaxDocChars));
			}
		}
		for (const FProtoBoyDocNode& Doc : Map.GetNodes())
		{
			if (Doc.Layer == EProtoBoyDocLayer::Content)
			{
				++NumContent;
				Out += FString::Printf(TEXT("### Content doc \"%s\" (under %s)\n\n%s\n"), *Doc.Title, *DescribeAncestors(Map, Doc),
					*ProtoBoyAgentPrompt::ReadDoc(Doc, MaxDocChars));
			}
		}
		if (NumContent == 0)
		{
			Out += TEXT("_There are no Content docs yet. Go by the request and the Game doc._\n\n");
		}
		return Out;
	}

	/** The user's filters, so the agent doesn't suggest what they rule out. */
	FString DescribeFilters(const FProtoBoyAssetSearch& Search)
	{
		FString Out;
		if (Search.Types.IsEmpty())
		{
			Out += TEXT("- Asset types: any.\n");
		}
		else
		{
			Out += FString::Printf(TEXT("- Asset types: only %s.\n"), *FString::JoinBy(Search.Types, TEXT(", "),
				[](EProtoBoyAssetType Type) { return FString::Printf(TEXT("`%s`"), *ProtoBoyAssetSourcing::GetAssetTypeKey(Type)); }));
		}
		if (Search.bFreeOnly)
		{
			Out += TEXT("- Price: free only.\n");
		}
		else if (Search.MinPrice.IsSet() || Search.MaxPrice.IsSet())
		{
			Out += FString::Printf(TEXT("- Price: %s to %s USD.\n"),
				Search.MinPrice.IsSet() ? *FString::SanitizeFloat(Search.MinPrice.GetValue(), 0) : TEXT("0"),
				Search.MaxPrice.IsSet() ? *FString::SanitizeFloat(Search.MaxPrice.GetValue(), 0) : TEXT("any"));
		}
		return Out;
	}

	/** Every type key with its display name, for the answer's "types". */
	FString DescribeTypeKeys()
	{
		TArray<FString> Keys;
		for (int32 Index = 0; Index < static_cast<int32>(EProtoBoyAssetType::Count); ++Index)
		{
			const EProtoBoyAssetType Type = static_cast<EProtoBoyAssetType>(Index);
			Keys.Add(FString::Printf(TEXT("`%s` (%s)"), *ProtoBoyAssetSourcing::GetAssetTypeKey(Type), *ProtoBoyAssetSourcing::GetAssetTypeName(Type).ToString()));
		}
		return FString::Join(Keys, TEXT(", "));
	}

	/** Names of the enabled sources, e.g. "Fab". */
	FString DescribeSources()
	{
		TArray<FString> Names;
		for (const FProtoBoyAssetSource& Source : ProtoBoyAssetSourcing::GetSources())
		{
			if (Source.IsEnabled())
			{
				Names.Add(Source.Name.ToString());
			}
		}
		return Names.IsEmpty() ? FString(TEXT("none")) : FString::Join(Names, TEXT(", "));
	}

	FString BuildPrompt(const FProtoBoyAssetSearch& Search)
	{
		FString Context;
		if (Search.bUseDocsContext)
		{
			Context += DescribeDocs();
		}
		if (Search.bUseProjectAssetsContext)
		{
			Context += TEXT("## Assets already in the project (Content/, from the asset registry)\n\n") + ProtoBoyAgentPrompt::DescribeProjectAssets() + TEXT("\n");
		}

		return FString::Printf(TEXT(R"MD(# Find assets on Fab for the Unreal Engine project "%s"

The user is looking for assets on these storefronts: %s. Turn their request into the specific assets they need, and
for each one find real, specific listings on Fab (pages like https://www.fab.com/listings/<id>) that fit.

## Rules

- Read only. Don't create, edit or delete any file, and don't run commands.
- You may look at the project (Content/ folder and file names, Config/, Source/) to see what's already there.
- Find listings with web search only, limited to fab.com (e.g. search "site:fab.com/listings stylized low poly pine
  trees"). Don't fetch pages. Only give listing URLs that appeared in your search results, exactly as they appeared;
  never make one up or guess an id. Skip search, category and seller pages: only /listings/<id> URLs count.
- As you work, print one short line per step that starts with "PROGRESS: ", e.g. "PROGRESS: Searching Fab for pine
  trees". Keep them short; they're shown to the user as a progress log.
- Don't suggest generating assets with AI%s.
- Match the style the docs ask for (the Content docs' style and What to Look For sections, the Game doc's pillars),
  and leave out anything their What to Avoid sections rule out.
- Skip assets the project already has, unless the request asks for more of them.
- Prefer listings that fit the user's filters below. Don't put prices in notes: ProtoBoy reads each listing's price
  from its page, and hides the ones outside the filters.
- "query" is what to type in Fab's search box to see more like it: 2-6 keywords (style + subject), not a sentence.
- Usually 3-6 assets, most useful first (one if the request names a single specific asset), each with 1-3 listings,
  best fit first. An asset with no fitting listing gets an empty "listings" list.

## The user's request

%s

## The user's filters

%s
%s## Your answer

When you're done, reply with only this JSON object inside a ```json fence, and nothing after it:

```json
{
  "assets": [
    {
      "name": "Short name of the asset",
      "description": "One or two sentences: what it's for and the style it must have.",
      "doc": "Title of the Content doc it comes from, or empty",
      "query": "stylized low poly pine trees",
      "types": ["model-3d"],
      "listings": [
        {
          "url": "https://www.fab.com/listings/<id from a search result>",
          "title": "The listing's title",
          "note": "One short line: why it fits."
        }
      ]
    }
  ]
}
```

"types" is a list of these keys (empty for any type): %s.
)MD"), FApp::GetProjectName(), *DescribeSources(),
			GetDefault<UProtoBoySettings>()->bPreferSourcedAssets ? TEXT(", and skip listings that say they were made with AI") : TEXT(""),
			*Search.Query.TrimStartAndEnd(), *DescribeFilters(Search), *Context, *DescribeTypeKeys());
	}
}

FProtoBoyAssetSearchRun::~FProtoBoyAssetSearchRun()
{
	Agent.Stop();
}

FString FProtoBoyAssetSearchRun::GetWorkDir()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ProtoBoy") / TEXT("AssetSearch"));
}

bool FProtoBoyAssetSearchRun::Start(EProtoBoyMCPClient Client, const FProtoBoyAssetSearch& Search, FText& OutError)
{
	if (IsRunning() || bFinished)
	{
		return false;
	}
	if (Search.Query.TrimStartAndEnd().IsEmpty())
	{
		OutError = LOCTEXT("NoQuery", "Enter what to search for.");
		return false;
	}
	Request = Search;

	if (Search.bUseDocsContext)
	{
		Agent.AddLog(LOCTEXT("LogReadingDocs", "Reading the Game doc and the Content docs for the agent...").ToString());
	}
	if (Search.bUseProjectAssetsContext)
	{
		Agent.AddLog(LOCTEXT("LogReadingAssets", "Summarizing the project's assets for the agent...").ToString());
	}
	FProtoBoyAgentRun::FParams Params;
	Params.Client = Client;
	Params.Prompt = ProtoBoyAssetSearch::BuildPrompt(Search);
	Params.WorkDir = GetWorkDir();
	Params.StartedMessage = FText::Format(LOCTEXT("LogStarted", "Started {0}. It reads the search context, then searches Fab for listings; this can take a few minutes."),
		FProtoBoyMCPClientConfig::GetClientName(Client));
	Params.AnswerMessage = LOCTEXT("LogWriting", "The agent is writing its suggestions...");
	// To find listing URLs. It still can't fetch pages: ProtoBoy reads each listing's preview itself.
	Params.bAllowWebSearch = true;
	return Agent.Start(Params, OutError);
}

void FProtoBoyAssetSearchRun::Tick()
{
	if (!bFinished && Agent.Tick())
	{
		Complete();
	}
}

void FProtoBoyAssetSearchRun::Complete()
{
	bFinished = true;
	const FString Json = Agent.ExtractJson();
	TSharedPtr<FJsonObject> Root;
	const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
	if (Json.IsEmpty())
	{
		Agent.AddLog(Agent.DescribeNoAnswer().ToString());
		return;
	}
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid() || !Root->TryGetArrayField(TEXT("assets"), Array))
	{
		Agent.AddLog(FText::Format(LOCTEXT("LogBadJson", "The agent's answer isn't the expected JSON. Its output is in {0}."), FText::FromString(Agent.GetOutputLogPath())).ToString());
		return;
	}

	TSet<FString> SeenIds;
	for (const TSharedPtr<FJsonValue>& Value : *Array)
	{
		const TSharedPtr<FJsonObject>* Asset = nullptr;
		if (!Value->TryGetObject(Asset) || Suggestions.Num() >= ProtoBoyAssetSearch::MaxSuggestions)
		{
			continue;
		}
		FProtoBoyAssetSuggestion Suggestion;
		(*Asset)->TryGetStringField(TEXT("name"), Suggestion.Name);
		(*Asset)->TryGetStringField(TEXT("description"), Suggestion.Description);
		(*Asset)->TryGetStringField(TEXT("doc"), Suggestion.DocTitle);
		Suggestion.Name = Suggestion.Name.TrimStartAndEnd();
		Suggestion.Description = Suggestion.Description.TrimStartAndEnd();
		Suggestion.DocTitle = Suggestion.DocTitle.TrimStartAndEnd();

		// The user's price filters stay; the agent sets the query and narrows the types.
		Suggestion.Search = Request;
		Suggestion.Search.bUseDocsContext = false;
		Suggestion.Search.bUseProjectAssetsContext = false;
		FString Query;
		(*Asset)->TryGetStringField(TEXT("query"), Query);
		Query = Query.Replace(TEXT("\r"), TEXT(" ")).Replace(TEXT("\n"), TEXT(" ")).TrimStartAndEnd();
		if (Query.IsEmpty())
		{
			continue;
		}
		Suggestion.Search.Query = Query;
		if (Suggestion.Name.IsEmpty())
		{
			Suggestion.Name = Query;
		}

		const TArray<TSharedPtr<FJsonValue>>* Types = nullptr;
		if ((*Asset)->TryGetArrayField(TEXT("types"), Types))
		{
			TArray<EProtoBoyAssetType> Picked;
			for (const TSharedPtr<FJsonValue>& TypeValue : *Types)
			{
				EProtoBoyAssetType Type;
				FString Key;
				// Only types the user allowed (none picked: any).
				if (TypeValue->TryGetString(Key) && ProtoBoyAssetSourcing::FromAssetTypeKey(Key, Type) && (Request.Types.IsEmpty() || Request.Types.Contains(Type)))
				{
					Picked.AddUnique(Type);
				}
			}
			if (!Picked.IsEmpty())
			{
				Suggestion.Search.Types = MoveTemp(Picked);
			}
		}

		// Only real Fab listing pages, each once across all suggestions.
		const TArray<TSharedPtr<FJsonValue>>* Listings = nullptr;
		if ((*Asset)->TryGetArrayField(TEXT("listings"), Listings))
		{
			for (const TSharedPtr<FJsonValue>& ListingValue : *Listings)
			{
				const TSharedPtr<FJsonObject>* ListingObject = nullptr;
				FString Url, Id;
				if (!ListingValue->TryGetObject(ListingObject) || Suggestion.Listings.Num() >= ProtoBoyAssetSearch::MaxListingsPerAsset)
				{
					continue;
				}
				(*ListingObject)->TryGetStringField(TEXT("url"), Url);
				if (!ProtoBoyFab::ParseListingUrl(Url, Id))
				{
					if (!Url.TrimStartAndEnd().IsEmpty())
					{
						Agent.AddLog(FString::Printf(TEXT("Skipped %s: not a Fab listing page."), *Url.TrimStartAndEnd()));
					}
					continue;
				}
				if (SeenIds.Contains(Id))
				{
					continue;
				}
				SeenIds.Add(Id);

				FProtoBoyAssetListing Listing;
				Listing.SourceName = TEXT("Fab");
				Listing.Id = Id;
				Listing.Url = ProtoBoyFab::MakeListingUrl(Id);
				(*ListingObject)->TryGetStringField(TEXT("title"), Listing.Title);
				(*ListingObject)->TryGetStringField(TEXT("note"), Listing.Note);
				Listing.Title = Listing.Title.TrimStartAndEnd();
				Listing.Note = Listing.Note.TrimStartAndEnd();
				Suggestion.Listings.Add(MoveTemp(Listing));
			}
		}
		Suggestions.Add(MoveTemp(Suggestion));
	}
	if (Suggestions.IsEmpty())
	{
		Agent.AddLog(LOCTEXT("LogNoAssets", "The agent didn't suggest any assets.").ToString());
		return;
	}
	bSucceeded = true;
	++Revision;

	int32 NumListings = 0;
	for (const FProtoBoyAssetSuggestion& Suggestion : Suggestions)
	{
		NumListings += Suggestion.Listings.Num();
	}
	Agent.AddLog(FText::Format(LOCTEXT("LogDone", "The agent suggested {0} {0}|plural(one=asset,other=assets) with {1} {1}|plural(one=listing,other=listings). Loading their previews from Fab..."),
		FText::AsNumber(Suggestions.Num()), FText::AsNumber(NumListings)).ToString());
	for (int32 SuggestionIndex = 0; SuggestionIndex < Suggestions.Num(); ++SuggestionIndex)
	{
		for (int32 ListingIndex = 0; ListingIndex < Suggestions[SuggestionIndex].Listings.Num(); ++ListingIndex)
		{
			LoadPreview(SuggestionIndex, ListingIndex);
		}
	}
}

FProtoBoyAssetListing* FProtoBoyAssetSearchRun::FindListing(int32 SuggestionIndex, int32 ListingIndex)
{
	return Suggestions.IsValidIndex(SuggestionIndex) && Suggestions[SuggestionIndex].Listings.IsValidIndex(ListingIndex)
		? &Suggestions[SuggestionIndex].Listings[ListingIndex]
		: nullptr;
}

void FProtoBoyAssetSearchRun::LoadPreview(int32 SuggestionIndex, int32 ListingIndex)
{
	const FProtoBoyAssetListing* Listing = FindListing(SuggestionIndex, ListingIndex);
	if (!Listing)
	{
		return;
	}
	++PendingLoads;
	const TWeakPtr<FProtoBoyAssetSearchRun> WeakThis = AsShared();
	ProtoBoyFab::FetchListingPreview(Listing->Id,
		[WeakThis, SuggestionIndex, ListingIndex](ProtoBoyFab::EListingFetch Result, const ProtoBoyFab::FListingPreview& Preview)
		{
			const TSharedPtr<FProtoBoyAssetSearchRun> This = WeakThis.Pin();
			if (!This.IsValid() || This->bCancelled)
			{
				return;
			}
			FProtoBoyAssetListing* Listing = This->FindListing(SuggestionIndex, ListingIndex);
			if (!Listing)
			{
				This->FinishLoad();
				return;
			}
			if (Result == ProtoBoyFab::EListingFetch::NotFound || (Result == ProtoBoyFab::EListingFetch::Found && Preview.Title.IsEmpty()))
			{
				Listing->bNotFound = true;
				This->Agent.AddLog(FString::Printf(TEXT("Skipped %s: there's no such listing."), *Listing->Url));
			}
			else if (Result == ProtoBoyFab::EListingFetch::Failed)
			{
				Listing->Preview = FProtoBoyAssetListing::EPreview::None;
			}
			else
			{
				Listing->Title = Preview.Title;
				Listing->ImageUrl = Preview.ImageUrl;
				Listing->Price = Preview.Price;
				Listing->Currency = Preview.Currency;
				Listing->ListPrice = Preview.ListPrice;

				// The agent can't always see prices in search results; the page can.
				const FProtoBoyAssetSearch& Filters = This->Request;
				if (Preview.Price.IsSet())
				{
					const double Price = Preview.Price.GetValue();
					Listing->bOutsideFilters = Filters.bFreeOnly
						? Price >= 0.005
						: (Filters.MinPrice.IsSet() && Price < Filters.MinPrice.GetValue()) || (Filters.MaxPrice.IsSet() && Price > Filters.MaxPrice.GetValue());
				}
				if (Listing->bOutsideFilters)
				{
					This->Agent.AddLog(FString::Printf(TEXT("Skipped \"%s\": %s is outside your price filter."), *Listing->Title,
						*ProtoBoyFab::FormatPrice(Preview.Price.GetValue(), Preview.Currency)));
				}
				else if (Preview.ImageUrl.IsEmpty())
				{
					Listing->Preview = FProtoBoyAssetListing::EPreview::None;
				}
				else
				{
					This->LoadImage(SuggestionIndex, ListingIndex);
				}
			}
			++This->Revision;
			This->FinishLoad();
		});
}

void FProtoBoyAssetSearchRun::LoadImage(int32 SuggestionIndex, int32 ListingIndex)
{
	const FProtoBoyAssetListing* Listing = FindListing(SuggestionIndex, ListingIndex);
	if (!Listing)
	{
		return;
	}
	++PendingLoads;
	const TWeakPtr<FProtoBoyAssetSearchRun> WeakThis = AsShared();
	const TMap<FString, FString> Headers = { { TEXT("Accept"), TEXT("image/*") } };
	ProtoBoyHttp::SendRequest(TEXT("GET"), Listing->ImageUrl, Headers, FString(),
		[WeakThis, SuggestionIndex, ListingIndex](const ProtoBoyHttp::FResponse& Response)
		{
			const TSharedPtr<FProtoBoyAssetSearchRun> This = WeakThis.Pin();
			if (!This.IsValid() || This->bCancelled)
			{
				return;
			}
			if (FProtoBoyAssetListing* Listing = This->FindListing(SuggestionIndex, ListingIndex))
			{
				if (Response.IsOk() && !Response.Content.IsEmpty())
				{
					Listing->ImageData = Response.Content;
					Listing->Preview = FProtoBoyAssetListing::EPreview::Ready;
				}
				else
				{
					Listing->Preview = FProtoBoyAssetListing::EPreview::None;
				}
				++This->Revision;
			}
			This->FinishLoad();
		});
}

void FProtoBoyAssetSearchRun::FinishLoad()
{
	PendingLoads = FMath::Max(0, PendingLoads - 1);
	if (PendingLoads > 0)
	{
		return;
	}
	int32 Shown = 0;
	for (const FProtoBoyAssetSuggestion& Suggestion : Suggestions)
	{
		Shown += Suggestion.Listings.FilterByPredicate([](const FProtoBoyAssetListing& Listing) { return Listing.IsShown(); }).Num();
	}
	Agent.AddLog(FText::Format(LOCTEXT("LogPreviewsDone", "Done: {0} {0}|plural(one=listing,other=listings) on Fab."), FText::AsNumber(Shown)).ToString());
}

void FProtoBoyAssetSearchRun::Cancel()
{
	if (bFinished)
	{
		// Stop loading previews; what's loaded stays.
		if (PendingLoads > 0)
		{
			bCancelled = true;
			PendingLoads = 0;
			++Revision;
		}
		return;
	}
	const bool bWasRunning = IsRunning();
	bCancelled = true;
	Agent.Stop();
	bFinished = true;
	bSucceeded = false;
	if (bWasRunning)
	{
		Agent.AddLog(LOCTEXT("LogCancelled", "Cancelled.").ToString());
	}
}

#undef LOCTEXT_NAMESPACE
