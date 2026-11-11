#pragma once

#include "Containers/Ticker.h"
#include "Tasks/PBTasksConnection.h"

class IHttpRouter;
struct FHttpRouteHandleInternal;
struct FHttpServerRequest;
struct FHttpServerResponse;

/**
 * Connects the user's Trello account with one click:
 *  1. BeginConnect opens trello.com/1/authorize in the browser with ProtoBoy's API key,
 *     redirecting back to a short-lived local page (http://127.0.0.1:<TasksCallbackPort>/trello/callback).
 *  2. The user clicks Allow. Trello redirects with the token in the URL fragment, which only the browser sees,
 *     so the page's script POSTs it to /trello/token along with a one-time state value.
 *  3. ProtoBoy checks the token against api.trello.com/1/members/me, saves it per user, and answers the page.
 *
 * The API key's Power-Up must list http://127.0.0.1:<TasksCallbackPort> under allowed origins, or Trello blocks the redirect.
 */
class PROTOBOY_API FProtoBoyTrelloConnector
{
public:
	enum class EState : uint8
	{
		Idle,
		/** Browser opened; waiting for the user to click Allow. */
		WaitingForBrowser,
		/** Token received; checking it with Trello. */
		Verifying,
	};

	FProtoBoyTrelloConnector();
	~FProtoBoyTrelloConnector();

	/** Start listening for the callback and open the browser. False with OutError if it can't start. */
	bool BeginConnect(FText& OutError);
	void CancelConnect();
	/** Forget the token (and revoke it on Trello, best effort). */
	void Disconnect();

	bool IsConnected() const { return Connection.Provider == EProtoBoyTaskProvider::Trello && !Connection.TrelloToken.IsEmpty(); }
	EState GetState() const { return State; }
	/** Why the last connect attempt failed; empty after success or a new attempt. */
	const FText& GetLastError() const { return LastError; }
	const FProtoBoyTasksConnection& GetConnection() const { return Connection; }

	/** UProtoBoySettings::TrelloApiKey, falling back to ProtoBoy's built-in key. */
	static FString GetApiKey();
	/** The origin to allow on the API key's Power-Up, e.g. http://127.0.0.1:47802 */
	static FString GetCallbackOrigin();

	/** Pump the callback listener during modal loops (the engine's core ticker is stalled there). */
	void TickWhileModal(float DeltaTime);

	/** Fires after connect/disconnect and on every state change, so UI can refresh. */
	FSimpleMulticastDelegate OnConnectionChanged;

private:
	/** Result of the members/me check, written on the HTTP thread and read on the game thread. */
	struct FVerifyResult
	{
		FCriticalSection Lock;
		bool bDone = false;
		bool bRequestOk = false;
		int32 ResponseCode = 0;
		FString Body;
	};

	bool HandleCallbackPage(const FHttpServerRequest& Request, const TFunction<void(TUniquePtr<FHttpServerResponse>&&)>& OnComplete);
	bool HandleToken(const FHttpServerRequest& Request, const TFunction<void(TUniquePtr<FHttpServerResponse>&&)>& OnComplete);

	void StartVerify(const FString& Token);
	/** Game-thread pump: finishes verification and times out abandoned attempts. */
	bool Pump(float DeltaTime);
	void FinishConnect(bool bSuccess, const FText& Message);

	void StopListening();
	void SetState(EState NewState);

	FProtoBoyTasksConnection Connection;
	EState State = EState::Idle;
	FText LastError;

	TSharedPtr<IHttpRouter> Router;
	TSharedPtr<const FHttpRouteHandleInternal> PageRoute;
	TSharedPtr<const FHttpRouteHandleInternal> TokenRoute;
	int32 ListeningPort = 0;

	/** One-time value carried through Trello's return_url; the page must echo it back. */
	FString StateNonce;
	FString PendingToken;
	FString PendingApiKey;
	/** A failure seen inside a route handler, finished on the next Pump (routes can't be unbound mid-dispatch). */
	TOptional<FText> DeferredFailure;
	/** The page's POST is answered once verification finishes. */
	TFunction<void(TUniquePtr<FHttpServerResponse>&&)> PendingReply;
	TSharedPtr<FVerifyResult> Verify;

	double WaitStartedAt = 0.0;
	FTSTicker::FDelegateHandle TickerHandle;
};
