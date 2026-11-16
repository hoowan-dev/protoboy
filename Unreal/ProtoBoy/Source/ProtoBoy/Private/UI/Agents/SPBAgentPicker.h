#pragma once

#include "MCP/PBMCPConnection.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Picks one of the user's agents to run unattended (FProtoBoyAgentRun): a radio button per agent set up in MCP
 * settings, with the agents that can't run (Claude Desktop) disabled and saying why, or a warning when none is set up.
 * Used by the generation windows (docs from the project, tasks from the docs).
 */
class SProtoBoyAgentPicker : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnAgentPicked, EProtoBoyMCPClient);

	SLATE_BEGIN_ARGS(SProtoBoyAgentPicker) {}
		/** The picked agent (unset = none). */
		SLATE_ATTRIBUTE(TOptional<EProtoBoyMCPClient>, SelectedAgent)
		SLATE_EVENT(FOnAgentPicked, OnAgentPicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** The first set-up agent that can run, to start with; unset if there's none. */
	static TOptional<EProtoBoyMCPClient> GetDefaultAgent();

private:
	TSharedRef<SWidget> MakeAgentRow(EProtoBoyMCPClient Agent);

	TAttribute<TOptional<EProtoBoyMCPClient>> SelectedAgent;
	FOnAgentPicked OnAgentPicked;
};
