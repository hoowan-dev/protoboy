#pragma once

#include "CoreMinimal.h"

/** Connection to the engine-agnostic ProtoBoy daemon (local WebSocket/IPC). */
class PROTOBOY_API FProtoBoyDaemonClient
{
public:
	void Connect(const FString& Url);
	void Disconnect();
	bool IsConnected() const { return bConnected; }

	/** Send a JSON message envelope of the given type. */
	void Send(const FString& Type, const FString& JsonPayload);

private:
	// TODO: IWebSocket via the WebSockets module, reconnect with backoff, bounded offline queue.
	bool bConnected = false;
};
