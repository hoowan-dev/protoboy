#include "Daemon/ProtoBoyDaemonClient.h"

#include "ProtoBoyModule.h"

void FProtoBoyDaemonClient::Connect(const FString& Url)
{
	// TODO
	UE_LOG(LogProtoBoy, Log, TEXT("Daemon client not implemented yet; staying offline (%s)."), *Url);
}

void FProtoBoyDaemonClient::Disconnect()
{
	bConnected = false;
}

void FProtoBoyDaemonClient::Send(const FString& Type, const FString& JsonPayload)
{
	// TODO
}
