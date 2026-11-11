#pragma once

#include "MCP/PBMCPConnection.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Provider page of the Connect LLM Provider window (Claude, Gemini, ...): one card per agent app.
 * Connect writes ProtoBoy's entry (URL + token) into the agent's config in one click; Copy is the manual fallback.
 */
class SProtoBoyMCPClientPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProtoBoyMCPClientPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, EProtoBoyLLMProvider InProvider);

private:
	TSharedRef<SWidget> MakeHeader();
	TSharedRef<SWidget> MakeClientCard(EProtoBoyMCPClient Client);

	FReply OnConnectClicked(EProtoBoyMCPClient Client);
	FReply OnDisconnectClicked(EProtoBoyMCPClient Client);
	FReply OnCopyClicked(EProtoBoyMCPClient Client);

	/** Re-read each client's config. Reading files isn't free, so this runs on open and after actions, not per frame. */
	void RefreshStatus();
	bool IsConfigured(EProtoBoyMCPClient Client) const;

	FString GetSnippet(EProtoBoyMCPClient Client, bool bRevealToken) const;

	EProtoBoyLLMProvider Provider = EProtoBoyLLMProvider::Claude;
	TMap<EProtoBoyMCPClient, bool> Configured;
	/** Last action result per client, shown under its buttons. */
	TMap<EProtoBoyMCPClient, FText> Messages;
	TMap<EProtoBoyMCPClient, bool> MessageIsError;
};
