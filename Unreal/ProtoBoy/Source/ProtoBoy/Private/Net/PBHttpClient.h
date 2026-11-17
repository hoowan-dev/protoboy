#pragma once

#include "CoreMinimal.h"

/**
 * Outgoing HTTP for ProtoBoy's connectors (Trello, ...), with callbacks delivered on the game thread.
 *
 * Requests complete on the HTTP thread and their callbacks are queued; PumpGameThreadQueue runs them. The subsystem pumps
 * from the core ticker and from Slate's modal loop tick, because the engine's own game-thread HTTP dispatch stalls while
 * a modal window is open.
 */
namespace ProtoBoyHttp
{
	struct FResponse
	{
		/** False if the request never got an HTTP response (offline, DNS, timeout). */
		bool bReachedServer = false;
		int32 Code = 0;
		FString Body;
		/** The body as bytes (images and other binary responses). */
		TArray<uint8> Content;
		/** The Retry-After header in seconds (rate limits), or 0 when there's none. */
		float RetryAfterSeconds = 0.f;

		bool IsOk() const { return bReachedServer && Code >= 200 && Code < 300; }
	};

	using FOnResponse = TFunction<void(const FResponse& Response)>;

	/** Send a request. Body is sent as JSON when non-empty. OnResponse runs on the game thread. */
	void SendRequest(const FString& Verb, const FString& Url, const TMap<FString, FString>& Headers, const FString& Body,
		FOnResponse OnResponse, float TimeoutSeconds = 20.f);

	/**
	 * Run Callback on the game thread after Seconds, from PumpGameThreadQueue (so it also runs while a modal window is
	 * open, unlike the core ticker). For retrying requests after a delay.
	 */
	void RunAfter(float Seconds, TFunction<void()> Callback);

	/** Run queued callbacks, and delayed ones that are due. Game thread only. */
	void PumpGameThreadQueue();
}
