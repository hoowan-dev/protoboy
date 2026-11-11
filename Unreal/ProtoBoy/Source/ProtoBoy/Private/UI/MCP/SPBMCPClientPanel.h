#pragma once

#include "MCP/PBMCPConnection.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Provider page of the Connect LLM Provider window (Claude, Gemini, ...):
 * one card per agent app with its config location, setup snippet, and Write Config / Copy / Test actions.
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

	FReply OnWriteConfigClicked(EProtoBoyMCPClient Client);
	FReply OnCopyClicked(EProtoBoyMCPClient Client);
	FReply OnTestClicked(EProtoBoyMCPClient Client);

	FString GetSnippet(EProtoBoyMCPClient Client, bool bRevealToken) const;

	EProtoBoyLLMProvider Provider = EProtoBoyLLMProvider::Claude;
	/** Last action result per client, shown under its buttons. */
	TMap<EProtoBoyMCPClient, FText> Messages;
};
