#include "Net/PBHttpClient.h"

#include "Containers/Queue.h"
#include "HAL/PlatformTime.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Modules/ModuleManager.h"

namespace ProtoBoyHttp
{
	/** Callbacks waiting for the game thread. Many HTTP threads produce, the game thread consumes. */
	static TQueue<TFunction<void()>, EQueueMode::Mpsc>& GetGameThreadQueue()
	{
		static TQueue<TFunction<void()>, EQueueMode::Mpsc> Queue;
		return Queue;
	}

	struct FDelayed
	{
		double DueSeconds = 0.0;
		TFunction<void()> Callback;
	};

	/** RunAfter's callbacks. Game thread only. */
	static TArray<FDelayed>& GetDelayed()
	{
		static TArray<FDelayed> Delayed;
		return Delayed;
	}

	void SendRequest(const FString& Verb, const FString& Url, const TMap<FString, FString>& Headers, const FString& Body,
		FOnResponse OnResponse, float TimeoutSeconds)
	{
		check(IsInGameThread());
		FModuleManager::Get().LoadModule(TEXT("HTTP"));

		const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
		Request->SetVerb(Verb);
		Request->SetURL(Url);
		Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
		for (const TPair<FString, FString>& Header : Headers)
		{
			Request->SetHeader(Header.Key, Header.Value);
		}
		if (!Body.IsEmpty())
		{
			Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
			Request->SetContentAsString(Body);
		}
		Request->SetTimeout(TimeoutSeconds);

		// Complete on the HTTP thread (the game-thread dispatch doesn't run in modal loops), then hop to our queue.
		Request->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnHttpThread);
		Request->OnProcessRequestComplete().BindLambda(
			[OnResponse = MoveTemp(OnResponse)](FHttpRequestPtr, FHttpResponsePtr HttpResponse, bool bProcessed) mutable
			{
				FResponse Result;
				Result.bReachedServer = bProcessed && HttpResponse.IsValid();
				Result.Code = HttpResponse.IsValid() ? HttpResponse->GetResponseCode() : 0;
				Result.Body = HttpResponse.IsValid() ? HttpResponse->GetContentAsString() : FString();
				if (HttpResponse.IsValid())
				{
					Result.Content = HttpResponse->GetContent();
				}
				if (HttpResponse.IsValid())
				{
					// Seconds only; an HTTP date falls back to the caller's own backoff.
					const FString RetryAfter = HttpResponse->GetHeader(TEXT("Retry-After"));
					Result.RetryAfterSeconds = RetryAfter.IsNumeric() ? FMath::Max(0.f, FCString::Atof(*RetryAfter)) : 0.f;
				}

				GetGameThreadQueue().Enqueue([OnResponse = MoveTemp(OnResponse), Result = MoveTemp(Result)]()
				{
					OnResponse(Result);
				});
			});
		Request->ProcessRequest();
	}

	void RunAfter(float Seconds, TFunction<void()> Callback)
	{
		check(IsInGameThread());
		GetDelayed().Add({ FPlatformTime::Seconds() + FMath::Max(0.f, Seconds), MoveTemp(Callback) });
	}

	void PumpGameThreadQueue()
	{
		check(IsInGameThread());
		TFunction<void()> Callback;
		while (GetGameThreadQueue().Dequeue(Callback))
		{
			Callback();
		}

		// Take the due ones out first: a callback may call RunAfter again.
		TArray<FDelayed>& Delayed = GetDelayed();
		if (Delayed.IsEmpty())
		{
			return;
		}
		const double Now = FPlatformTime::Seconds();
		TArray<TFunction<void()>> Due;
		for (int32 Index = Delayed.Num() - 1; Index >= 0; --Index)
		{
			if (Delayed[Index].DueSeconds <= Now)
			{
				Due.Insert(MoveTemp(Delayed[Index].Callback), 0);
				Delayed.RemoveAt(Index);
			}
		}
		for (TFunction<void()>& Run : Due)
		{
			Run();
		}
	}
}
