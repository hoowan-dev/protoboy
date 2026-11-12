#include "Net/PBHttpClient.h"

#include "Containers/Queue.h"
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

				GetGameThreadQueue().Enqueue([OnResponse = MoveTemp(OnResponse), Result = MoveTemp(Result)]()
				{
					OnResponse(Result);
				});
			});
		Request->ProcessRequest();
	}

	void PumpGameThreadQueue()
	{
		check(IsInGameThread());
		TFunction<void()> Callback;
		while (GetGameThreadQueue().Dequeue(Callback))
		{
			Callback();
		}
	}
}
