#include "Sourcing/Fab/PBFabListings.h"

#include "Dom/JsonObject.h"
#include "Misc/Guid.h"
#include "Net/PBHttpClient.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace ProtoBoyFabListings
{
	/** Undo the HTML entities preview tags use. */
	FString DecodeEntities(FString Text)
	{
		Text.ReplaceInline(TEXT("&quot;"), TEXT("\""));
		Text.ReplaceInline(TEXT("&#39;"), TEXT("'"));
		Text.ReplaceInline(TEXT("&#x27;"), TEXT("'"));
		Text.ReplaceInline(TEXT("&lt;"), TEXT("<"));
		Text.ReplaceInline(TEXT("&gt;"), TEXT(">"));
		// Last, so "&amp;lt;" stays "&lt;".
		Text.ReplaceInline(TEXT("&amp;"), TEXT("&"));
		return Text.TrimStartAndEnd();
	}

	/** The content of <meta property="Property" content="..."> (attributes in any order), or empty. */
	FString FindMetaContent(const FString& Html, const TCHAR* Property)
	{
		const FString Needle = FString::Printf(TEXT("\"%s\""), Property);
		const int32 At = Html.Find(Needle, ESearchCase::IgnoreCase);
		if (At == INDEX_NONE)
		{
			return FString();
		}
		const int32 TagStart = Html.Find(TEXT("<meta"), ESearchCase::IgnoreCase, ESearchDir::FromEnd, At);
		const int32 TagEnd = Html.Find(TEXT(">"), ESearchCase::CaseSensitive, ESearchDir::FromStart, At);
		if (TagStart == INDEX_NONE || TagEnd == INDEX_NONE)
		{
			return FString();
		}
		const FString Tag = Html.Mid(TagStart, TagEnd - TagStart);
		const int32 ContentAt = Tag.Find(TEXT("content=\""), ESearchCase::IgnoreCase);
		if (ContentAt == INDEX_NONE)
		{
			return FString();
		}
		const int32 ValueStart = ContentAt + 9;
		const int32 ValueEnd = Tag.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, ValueStart);
		return ValueEnd == INDEX_NONE ? FString() : DecodeEntities(Tag.Mid(ValueStart, ValueEnd - ValueStart));
	}

	/** A number or numeric string field. */
	bool TryGetNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, double& OutValue)
	{
		if (Object->TryGetNumberField(Field, OutValue))
		{
			return true;
		}
		FString Text;
		if (Object->TryGetStringField(Field, Text) && Text.IsNumeric())
		{
			OutValue = FCString::Atod(*Text);
			return true;
		}
		return false;
	}

	/** The price in the page's schema.org Product data (<script type="application/ld+json">): its offer, or its cheapest. */
	void ReadStructuredPrice(const FString& Html, ProtoBoyFab::FListingPreview& OutPreview)
	{
		const FString Open = TEXT("<script type=\"application/ld+json\">");
		int32 From = 0;
		while (From < Html.Len())
		{
			const int32 Start = Html.Find(Open, ESearchCase::IgnoreCase, ESearchDir::FromStart, From);
			if (Start == INDEX_NONE)
			{
				return;
			}
			const int32 JsonStart = Start + Open.Len();
			const int32 End = JsonStart < Html.Len() ? Html.Find(TEXT("</script>"), ESearchCase::IgnoreCase, ESearchDir::FromStart, JsonStart) : INDEX_NONE;
			if (End == INDEX_NONE)
			{
				return;
			}
			From = End + 9;

			TSharedPtr<FJsonObject> Root;
			FString Type;
			if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Html.Mid(JsonStart, End - JsonStart)), Root) || !Root.IsValid()
				|| !Root->TryGetStringField(TEXT("@type"), Type) || Type != TEXT("Product"))
			{
				continue;
			}
			TArray<TSharedPtr<FJsonObject>> Offers;
			const TSharedPtr<FJsonObject>* Offer = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* OfferArray = nullptr;
			if (Root->TryGetObjectField(TEXT("offers"), Offer))
			{
				Offers.Add(*Offer);
			}
			else if (Root->TryGetArrayField(TEXT("offers"), OfferArray))
			{
				for (const TSharedPtr<FJsonValue>& Value : *OfferArray)
				{
					if (Value->TryGetObject(Offer))
					{
						Offers.Add(*Offer);
					}
				}
			}
			for (const TSharedPtr<FJsonObject>& Each : Offers)
			{
				double Price = 0.0;
				if (TryGetNumber(Each, TEXT("price"), Price) && (!OutPreview.Price.IsSet() || Price < OutPreview.Price.GetValue()))
				{
					OutPreview.Price = Price;
					Each->TryGetStringField(TEXT("priceCurrency"), OutPreview.Currency);
				}
			}
			if (OutPreview.Price.IsSet())
			{
				return;
			}
		}
	}

	/**
	 * The price before a sale: the first "price" after "startingPrice" in the page's own data, which also holds the
	 * discounted price. Best effort; the sale just isn't shown when the page's data changes shape.
	 */
	void ReadListPrice(const FString& Html, ProtoBoyFab::FListingPreview& OutPreview)
	{
		if (!OutPreview.Price.IsSet())
		{
			return;
		}
		const int32 At = Html.Find(TEXT("startingPrice"), ESearchCase::CaseSensitive);
		const int32 PriceAt = At == INDEX_NONE ? INDEX_NONE : Html.Find(TEXT("price"), ESearchCase::CaseSensitive, ESearchDir::FromStart, At + 13);
		if (PriceAt == INDEX_NONE || PriceAt - At > 300)
		{
			return;
		}
		// Past `price\": ` to the number.
		int32 Index = PriceAt + 5;
		while (Index < Html.Len() && Index < PriceAt + 12 && !FChar::IsDigit(Html[Index]))
		{
			++Index;
		}
		int32 NumberEnd = Index;
		while (NumberEnd < Html.Len() && (FChar::IsDigit(Html[NumberEnd]) || Html[NumberEnd] == TEXT('.')))
		{
			++NumberEnd;
		}
		if (NumberEnd == Index)
		{
			return;
		}
		const double ListPrice = FCString::Atod(*Html.Mid(Index, NumberEnd - Index));
		if (ListPrice > OutPreview.Price.GetValue() + 0.005)
		{
			OutPreview.ListPrice = ListPrice;
		}
	}
}

namespace ProtoBoyFab
{
	bool ParseListingUrl(const FString& Url, FString& OutId)
	{
		FString Rest = Url.TrimStartAndEnd();
		if (!Rest.RemoveFromStart(TEXT("https://"), ESearchCase::IgnoreCase))
		{
			Rest.RemoveFromStart(TEXT("http://"), ESearchCase::IgnoreCase);
		}
		Rest.RemoveFromStart(TEXT("www."), ESearchCase::IgnoreCase);
		if (!Rest.RemoveFromStart(TEXT("fab.com/"), ESearchCase::IgnoreCase))
		{
			return false;
		}
		int32 Cut = INDEX_NONE;
		if (Rest.FindChar(TEXT('?'), Cut) || Rest.FindChar(TEXT('#'), Cut))
		{
			Rest.LeftInline(Cut);
		}

		// "listings/<id>" or "<locale>/listings/<id>".
		TArray<FString> Parts;
		Rest.ParseIntoArray(Parts, TEXT("/"));
		const int32 ListingsAt = Parts.IndexOfByKey(TEXT("listings"));
		if (ListingsAt == INDEX_NONE || ListingsAt > 1 || !Parts.IsValidIndex(ListingsAt + 1))
		{
			return false;
		}
		FGuid Guid;
		if (!FGuid::ParseExact(Parts[ListingsAt + 1], EGuidFormats::DigitsWithHyphens, Guid))
		{
			return false;
		}
		OutId = Guid.ToString(EGuidFormats::DigitsWithHyphensLower);
		return true;
	}

	FString MakeListingUrl(const FString& Id)
	{
		return FString::Printf(TEXT("https://www.fab.com/listings/%s"), *Id);
	}

	void FetchListingPreview(const FString& Id, TFunction<void(EListingFetch Result, const FListingPreview& Preview)> OnDone)
	{
		const TMap<FString, FString> Headers = {
			{ TEXT("Accept"), TEXT("text/html") },
			{ TEXT("User-Agent"), TEXT("ProtoBoy (Unreal Editor plugin; link preview)") },
		};
		ProtoBoyHttp::SendRequest(TEXT("GET"), MakeListingUrl(Id), Headers, FString(),
			[OnDone = MoveTemp(OnDone)](const ProtoBoyHttp::FResponse& Response)
			{
				FListingPreview Preview;
				if (Response.bReachedServer && Response.Code == 404)
				{
					OnDone(EListingFetch::NotFound, Preview);
					return;
				}
				if (!Response.IsOk())
				{
					OnDone(EListingFetch::Failed, Preview);
					return;
				}
				Preview.Title = ProtoBoyFabListings::FindMetaContent(Response.Body, TEXT("og:title"));
				Preview.ImageUrl = ProtoBoyFabListings::FindMetaContent(Response.Body, TEXT("og:image"));
				ProtoBoyFabListings::ReadStructuredPrice(Response.Body, Preview);
				ProtoBoyFabListings::ReadListPrice(Response.Body, Preview);
				OnDone(EListingFetch::Found, Preview);
			});
	}

	FString FormatPrice(double Price, const FString& Currency)
	{
		if (Price < 0.005)
		{
			return TEXT("Free");
		}
		return Currency.IsEmpty() || Currency == TEXT("USD")
			? FString::Printf(TEXT("$%.2f"), Price)
			: FString::Printf(TEXT("%.2f %s"), Price, *Currency);
	}
}
