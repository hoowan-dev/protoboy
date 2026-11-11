#include "Tasks/Trello/PBTrelloConnector.h"

#include "Dom/JsonObject.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HAL/PlatformProcess.h"
#include "HttpModule.h"
#include "HttpPath.h"
#include "HttpServerModule.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "Modules/ModuleManager.h"
#include "Net/PBHttpServerUtils.h"
#include "PBModule.h"
#include "PBSettings.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#define LOCTEXT_NAMESPACE "ProtoBoyTrelloConnector"

namespace ProtoBoyTrello
{
	/**
	 * TODO: ProtoBoy's own Trello API key (from its Power-Up at trello.com/power-ups/admin), so users don't need one.
	 * An API key is public by design; never put the Power-Up's secret here.
	 */
	const TCHAR* BuiltInApiKey = TEXT("");

	/** Give up on a browser round-trip the user walked away from. */
	constexpr double BrowserTimeoutSeconds = 10.0 * 60.0;

	const TCHAR* CallbackPath = TEXT("/trello/callback");
	const TCHAR* TokenPath = TEXT("/trello/token");

	/** Page Trello redirects to. Reads the token from the fragment and hands it to ProtoBoy. */
	const TCHAR* CallbackPage = TEXT(R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="referrer" content="no-referrer">
<title>ProtoBoy - Trello</title>
<style>
  :root { color-scheme: light dark; }
  body { margin: 0; min-height: 100vh; display: flex; align-items: center; justify-content: center;
         font-family: system-ui, -apple-system, "Segoe UI", sans-serif; background: #1a1a1a; color: #e6e6e6; }
  main { max-width: 28rem; padding: 2rem; text-align: center; }
  h1 { font-size: 1.4rem; margin: 0 0 .5rem; }
  p { margin: 0; color: #a8a8a8; line-height: 1.5; }
  .ok h1 { color: #4ec27a; } .err h1 { color: #ef6a6a; }
</style>
</head>
<body>
<main id="box">
  <h1 id="title">Connecting to ProtoBoy...</h1>
  <p id="msg">Checking your Trello authorization.</p>
</main>
<script>
(function () {
  var hash = new URLSearchParams(location.hash.slice(1));
  var query = new URLSearchParams(location.search);
  // Keep the token out of the address bar and history.
  history.replaceState(null, "", location.pathname);

  function show(ok, title, msg) {
    document.getElementById("box").className = ok ? "ok" : "err";
    document.getElementById("title").textContent = title;
    document.getElementById("msg").textContent = msg;
  }

  fetch("/trello/token", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ token: hash.get("token") || "", error: hash.get("error") || "", state: query.get("state") || "" })
  })
  .then(function (r) { return r.json(); })
  .then(function (j) { show(j.ok, j.ok ? "Connected to Trello" : "Couldn't connect", j.message); })
  .catch(function () { show(false, "Couldn't reach ProtoBoy", "Make sure the Unreal Editor is still open, then try Connect again."); });
})();
</script>
</body>
</html>
)HTML");

	TUniquePtr<FHttpServerResponse> MakeJsonReply(bool bOk, const FText& Message, EHttpServerResponseCodes Code = EHttpServerResponseCodes::Ok)
	{
		TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
		Body->SetBoolField(TEXT("ok"), bOk);
		Body->SetStringField(TEXT("message"), Message.ToString());

		FString Text;
		FJsonSerializer::Serialize(Body, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
		TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(Text, TEXT("application/json"));
		Response->Code = Code;
		Response->Headers.Add(TEXT("Cache-Control"), { TEXT("no-store") });
		return Response;
	}

	/** Trello tokens are alphanumeric; reject anything else before it goes into a header. */
	bool LooksLikeToken(const FString& Token)
	{
		if (Token.Len() < 32 || Token.Len() > 256)
		{
			return false;
		}
		for (const TCHAR C : Token)
		{
			if (!FChar::IsAlnum(C))
			{
				return false;
			}
		}
		return true;
	}

	/** Trello accepts key + token in an OAuth-style header, which keeps the token out of URLs and logs. */
	FString MakeAuthHeader(const FString& ApiKey, const FString& Token)
	{
		return FString::Printf(TEXT("OAuth oauth_consumer_key=\"%s\", oauth_token=\"%s\""), *ApiKey, *Token);
	}
}

FProtoBoyTrelloConnector::FProtoBoyTrelloConnector()
{
	FProtoBoyTasksConnectionStore::Load(Connection);
}

FProtoBoyTrelloConnector::~FProtoBoyTrelloConnector()
{
	if (PendingReply)
	{
		PendingReply(ProtoBoyTrello::MakeJsonReply(false, LOCTEXT("EditorClosing", "The Unreal Editor is shutting down.")));
		PendingReply = nullptr;
	}
	StopListening();
}

FString FProtoBoyTrelloConnector::GetApiKey()
{
	const FString& Configured = GetDefault<UProtoBoySettings>()->TrelloApiKey;
	return Configured.TrimStartAndEnd().IsEmpty() ? FString(ProtoBoyTrello::BuiltInApiKey) : Configured.TrimStartAndEnd();
}

FString FProtoBoyTrelloConnector::GetCallbackOrigin()
{
	return FString::Printf(TEXT("http://127.0.0.1:%d"), GetDefault<UProtoBoySettings>()->TasksCallbackPort);
}

bool FProtoBoyTrelloConnector::BeginConnect(FText& OutError)
{
	CancelConnect();
	LastError = FText::GetEmpty();

	const FString ApiKey = GetApiKey();
	if (ApiKey.IsEmpty())
	{
		OutError = LOCTEXT("NoApiKey", "No Trello API key is set. Add one under Project Settings > Plugins > ProtoBoy > Tasks.");
		return false;
	}

	if (!FModuleManager::Get().LoadModule(TEXT("HTTPServer")))
	{
		OutError = LOCTEXT("NoHttpServer", "The engine's HTTPServer module couldn't be loaded.");
		return false;
	}

	const int32 Port = GetDefault<UProtoBoySettings>()->TasksCallbackPort;
	FHttpServerModule& HttpServer = FHttpServerModule::Get();
	HttpServer.SetListenerBindAddress(Port, TEXT("127.0.0.1"));

	Router = HttpServer.GetHttpRouter(Port, /*bFailOnBindFailure*/ true);
	if (!Router.IsValid())
	{
		OutError = FText::Format(LOCTEXT("BindFailed", "Couldn't listen on port {0} for Trello's reply. Another program may be using it."),
			FText::AsNumber(Port, &FNumberFormattingOptions::DefaultNoGrouping()));
		return false;
	}

	PageRoute = Router->BindRoute(FHttpPath(ProtoBoyTrello::CallbackPath), EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateRaw(this, &FProtoBoyTrelloConnector::HandleCallbackPage));
	TokenRoute = Router->BindRoute(FHttpPath(ProtoBoyTrello::TokenPath), EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateRaw(this, &FProtoBoyTrelloConnector::HandleToken));
	if (!PageRoute.IsValid() || !TokenRoute.IsValid())
	{
		StopListening();
		OutError = LOCTEXT("RouteTaken", "Another Trello connection attempt is already listening.");
		return false;
	}
	HttpServer.StartAllListeners();
	ListeningPort = Port;

	StateNonce = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
	PendingApiKey = ApiKey;

	const FString ReturnUrl = FString::Printf(TEXT("%s%s?state=%s"), *GetCallbackOrigin(), ProtoBoyTrello::CallbackPath, *StateNonce);
	const FString AuthorizeUrl = FString::Printf(
		TEXT("https://trello.com/1/authorize?expiration=never&name=ProtoBoy&scope=read,write&response_type=token&callback_method=fragment&key=%s&return_url=%s"),
		*FGenericPlatformHttp::UrlEncode(ApiKey), *FGenericPlatformHttp::UrlEncode(ReturnUrl));

	FString LaunchError;
	FPlatformProcess::LaunchURL(*AuthorizeUrl, nullptr, &LaunchError);
	if (!LaunchError.IsEmpty())
	{
		StopListening();
		OutError = FText::Format(LOCTEXT("LaunchFailed", "Couldn't open the browser: {0}"), FText::FromString(LaunchError));
		return false;
	}

	WaitStartedAt = FPlatformTime::Seconds();
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FProtoBoyTrelloConnector::Pump));
	SetState(EState::WaitingForBrowser);
	return true;
}

void FProtoBoyTrelloConnector::CancelConnect()
{
	if (State == EState::Idle)
	{
		return;
	}

	if (PendingReply)
	{
		PendingReply(ProtoBoyTrello::MakeJsonReply(false, LOCTEXT("Cancelled", "Connecting was cancelled in Unreal.")));
		PendingReply = nullptr;
	}
	StopListening();
	SetState(EState::Idle);
}

void FProtoBoyTrelloConnector::Disconnect()
{
	CancelConnect();
	if (!IsConnected())
	{
		return;
	}

	// Best effort: revoke the token so it stops working everywhere, not just here.
	if (FModuleManager::Get().LoadModule(TEXT("HTTP")))
	{
		const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
		Request->SetURL(FString::Printf(TEXT("https://api.trello.com/1/tokens/%s/"), *Connection.TrelloToken));
		Request->SetVerb(TEXT("DELETE"));
		Request->SetHeader(TEXT("Authorization"), ProtoBoyTrello::MakeAuthHeader(Connection.TrelloApiKey, Connection.TrelloToken));
		Request->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnHttpThread);
		Request->ProcessRequest();
	}

	Connection = FProtoBoyTasksConnection();
	FProtoBoyTasksConnectionStore::Clear();
	UE_LOG(LogProtoBoy, Log, TEXT("Trello disconnected."));
	OnConnectionChanged.Broadcast();
}

void FProtoBoyTrelloConnector::TickWhileModal(float DeltaTime)
{
	if (ListeningPort != 0 && FHttpServerModule::IsAvailable())
	{
		FHttpServerModule::Get().TickListener(ListeningPort, DeltaTime);
	}
	if (State != EState::Idle)
	{
		Pump(DeltaTime);
	}
}

bool FProtoBoyTrelloConnector::HandleCallbackPage(const FHttpServerRequest& Request, const TFunction<void(TUniquePtr<FHttpServerResponse>&&)>& OnComplete)
{
	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(FString(ProtoBoyTrello::CallbackPage), TEXT("text/html; charset=utf-8"));
	Response->Headers.Add(TEXT("Cache-Control"), { TEXT("no-store") });
	Response->Headers.Add(TEXT("Referrer-Policy"), { TEXT("no-referrer") });
	OnComplete(MoveTemp(Response));
	return true;
}

bool FProtoBoyTrelloConnector::HandleToken(const FHttpServerRequest& Request, const TFunction<void(TUniquePtr<FHttpServerResponse>&&)>& OnComplete)
{
	// Only our own callback page (same origin) may hand over a token.
	const FString Origin = ProtoBoyHttp::GetHeader(Request, TEXT("Origin"));
	if (!Origin.IsEmpty() && Origin != GetCallbackOrigin())
	{
		OnComplete(ProtoBoyTrello::MakeJsonReply(false, LOCTEXT("BadOrigin", "Request refused."), EHttpServerResponseCodes::Forbidden));
		return true;
	}

	if (State != EState::WaitingForBrowser || DeferredFailure.IsSet())
	{
		OnComplete(ProtoBoyTrello::MakeJsonReply(false, State == EState::Verifying
			? LOCTEXT("AlreadyVerifying", "ProtoBoy is already checking a Trello authorization.")
			: LOCTEXT("NotWaiting", "ProtoBoy isn't waiting for Trello right now. Click Connect in Unreal and try again.")));
		return true;
	}

	const FString Body = ProtoBoyHttp::GetBodyAsString(Request);

	TSharedPtr<FJsonObject> Json;
	FString Token, Error, ReturnedState;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Json) || !Json.IsValid())
	{
		OnComplete(ProtoBoyTrello::MakeJsonReply(false, LOCTEXT("BadBody", "Malformed request."), EHttpServerResponseCodes::BadRequest));
		return true;
	}
	Json->TryGetStringField(TEXT("token"), Token);
	Json->TryGetStringField(TEXT("error"), Error);
	Json->TryGetStringField(TEXT("state"), ReturnedState);

	if (ReturnedState.IsEmpty() || ReturnedState != StateNonce)
	{
		OnComplete(ProtoBoyTrello::MakeJsonReply(false, LOCTEXT("BadState", "This authorization doesn't match the one Unreal started. Click Connect again."),
			EHttpServerResponseCodes::BadRequest));
		return true;
	}

	if (!Error.IsEmpty() || Token.IsEmpty())
	{
		OnComplete(ProtoBoyTrello::MakeJsonReply(false, LOCTEXT("Denied", "Access wasn't granted in Trello. Click Connect in Unreal to try again.")));
		// Finish on the next pump, not while the router is dispatching to this route.
		DeferredFailure = LOCTEXT("DeniedShort", "Access wasn't granted in Trello.");
		return true;
	}

	if (!ProtoBoyTrello::LooksLikeToken(Token))
	{
		OnComplete(ProtoBoyTrello::MakeJsonReply(false, LOCTEXT("BadToken", "Trello returned an unexpected token.")));
		DeferredFailure = LOCTEXT("BadTokenShort", "Trello returned an unexpected token.");
		return true;
	}

	// Answer the page once Trello confirms the token.
	PendingReply = OnComplete;
	StartVerify(Token);
	return true;
}

void FProtoBoyTrelloConnector::StartVerify(const FString& Token)
{
	PendingToken = Token;
	Verify = MakeShared<FVerifyResult>();
	SetState(EState::Verifying);

	FModuleManager::Get().LoadModule(TEXT("HTTP"));
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(TEXT("https://api.trello.com/1/members/me?fields=username,fullName"));
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), ProtoBoyTrello::MakeAuthHeader(PendingApiKey, Token));
	Request->SetTimeout(20.f);

	// Complete on the HTTP thread: the game-thread dispatch doesn't run during modal loops. Pump() picks the result up.
	Request->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnHttpThread);
	TWeakPtr<FVerifyResult> WeakResult = Verify;
	Request->OnProcessRequestComplete().BindLambda([WeakResult](FHttpRequestPtr, FHttpResponsePtr Response, bool bProcessed)
	{
		if (const TSharedPtr<FVerifyResult> Result = WeakResult.Pin())
		{
			FScopeLock Lock(&Result->Lock);
			Result->bRequestOk = bProcessed && Response.IsValid();
			Result->ResponseCode = Response.IsValid() ? Response->GetResponseCode() : 0;
			Result->Body = Response.IsValid() ? Response->GetContentAsString() : FString();
			Result->bDone = true;
		}
	});
	Request->ProcessRequest();
}

bool FProtoBoyTrelloConnector::Pump(float DeltaTime)
{
	if (DeferredFailure.IsSet())
	{
		const FText Message = DeferredFailure.GetValue();
		DeferredFailure.Reset();
		FinishConnect(false, Message);
		return false;
	}

	if (State == EState::WaitingForBrowser && FPlatformTime::Seconds() - WaitStartedAt > ProtoBoyTrello::BrowserTimeoutSeconds)
	{
		FinishConnect(false, LOCTEXT("TimedOut", "Timed out waiting for Trello. Click Connect to try again."));
		return false;
	}

	if (State != EState::Verifying || !Verify.IsValid())
	{
		return State != EState::Idle;
	}

	bool bRequestOk = false;
	int32 Code = 0;
	FString Body;
	{
		FScopeLock Lock(&Verify->Lock);
		if (!Verify->bDone)
		{
			return true;
		}
		bRequestOk = Verify->bRequestOk;
		Code = Verify->ResponseCode;
		Body = Verify->Body;
	}

	if (!bRequestOk)
	{
		FinishConnect(false, LOCTEXT("NoNetwork", "Couldn't reach api.trello.com. Check your connection and try again."));
		return false;
	}
	if (Code == 401)
	{
		FinishConnect(false, LOCTEXT("Rejected", "Trello rejected the token. Try connecting again."));
		return false;
	}
	if (Code != 200)
	{
		FinishConnect(false, FText::Format(LOCTEXT("HttpError", "Trello answered with HTTP {0}."), FText::AsNumber(Code)));
		return false;
	}

	TSharedPtr<FJsonObject> Member;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Member) || !Member.IsValid())
	{
		FinishConnect(false, LOCTEXT("BadMember", "Trello sent an unexpected reply."));
		return false;
	}

	Connection = FProtoBoyTasksConnection();
	Connection.Provider = EProtoBoyTaskProvider::Trello;
	Connection.TrelloToken = PendingToken;
	Connection.TrelloApiKey = PendingApiKey;
	Member->TryGetStringField(TEXT("id"), Connection.TrelloMemberId);
	Member->TryGetStringField(TEXT("username"), Connection.TrelloUsername);
	Member->TryGetStringField(TEXT("fullName"), Connection.TrelloFullName);
	Connection.ConnectedAtUtc = FDateTime::UtcNow().ToIso8601();
	FProtoBoyTasksConnectionStore::Save(Connection);

	UE_LOG(LogProtoBoy, Log, TEXT("Trello connected as @%s."), *Connection.TrelloUsername);
	FinishConnect(true, FText::Format(LOCTEXT("Connected", "Connected as @{0}. You can close this tab and return to Unreal."),
		FText::FromString(Connection.TrelloUsername)));
	return false;
}

void FProtoBoyTrelloConnector::FinishConnect(bool bSuccess, const FText& Message)
{
	if (PendingReply)
	{
		PendingReply(ProtoBoyTrello::MakeJsonReply(bSuccess, Message));
		PendingReply = nullptr;
	}

	LastError = bSuccess ? FText::GetEmpty() : Message;
	if (!bSuccess)
	{
		UE_LOG(LogProtoBoy, Warning, TEXT("Trello connect failed: %s"), *Message.ToString());
	}

	StopListening();
	SetState(EState::Idle);
}

void FProtoBoyTrelloConnector::StopListening()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}

	// The engine keeps the port's listener open (no per-port stop); unbinding makes it answer 404.
	if (Router.IsValid() && FHttpServerModule::IsAvailable())
	{
		if (PageRoute.IsValid())
		{
			Router->UnbindRoute(PageRoute);
		}
		if (TokenRoute.IsValid())
		{
			Router->UnbindRoute(TokenRoute);
		}
	}
	PageRoute.Reset();
	TokenRoute.Reset();
	Router.Reset();
	ListeningPort = 0;

	StateNonce.Reset();
	DeferredFailure.Reset();
	PendingToken.Reset();
	PendingApiKey.Reset();
	Verify.Reset();
}

void FProtoBoyTrelloConnector::SetState(EState NewState)
{
	State = NewState;
	OnConnectionChanged.Broadcast();
}

#undef LOCTEXT_NAMESPACE
