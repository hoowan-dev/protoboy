#pragma once

#include "CoreMinimal.h"
#include "MCP/PBAgentRun.h"

class FProtoBoyDocMap;

/**
 * Generates the project's docs from what's already in it, with one of the user's agents (Set Up Docs from Template >
 * Generate from the Project), through FProtoBoyAgentRun. The prompt includes a summary of the Content folder from the
 * asset registry (the agent can't read .uasset files) and asks for the doc set as JSON in the template format
 * (FProtoBoyDocTemplate, sections as Examples). ProtoBoy then writes it with FProtoBoyDocMap::ApplyTemplate, so the
 * agent never writes files itself.
 *
 * Starting clears the docs on the map (to the provider's trash). Cancelling stops the agent and clears them again, so a
 * cancelled run leaves no docs. Tick() is polled by the UI (the window is modal, so the core ticker doesn't run).
 * The prompt and the agent's raw output are kept in Saved/ProtoBoy/DocGeneration/ for troubleshooting.
 */
class PROTOBOY_API FProtoBoyDocGenerationRun
{
public:
	explicit FProtoBoyDocGenerationRun(FProtoBoyDocMap& InMap);
	~FProtoBoyDocGenerationRun();

	/** The agents this user has set up (FProtoBoyAgentRun::GetSetUpAgents). */
	static TArray<EProtoBoyMCPClient> GetSetUpAgents() { return FProtoBoyAgentRun::GetSetUpAgents(); }
	/** False with a reason for agents that can't run unattended (FProtoBoyAgentRun::CanRun). */
	static bool CanRun(EProtoBoyMCPClient Client, FText& OutWhyNot) { return FProtoBoyAgentRun::CanRun(Client, OutWhyNot); }

	/** Clear the docs, write the prompt and start the agent. False (nothing started) with a reason. */
	bool Start(EProtoBoyMCPClient Client, FText& OutError);
	/** Read the agent's output, and once it exits, write the docs it returned. Call often while running. */
	void Tick();
	/** Stop the agent (if it's still running) and clear the docs. */
	void Cancel();

	bool IsRunning() const { return Agent.IsRunning(); }
	bool IsFinished() const { return bFinished; }
	bool Succeeded() const { return bSucceeded; }

	/** Every progress line so far, oldest first. */
	const TArray<FString>& GetLog() const { return Agent.GetLog(); }

	/** Saved/ProtoBoy/DocGeneration/. */
	static FString GetWorkDir();

private:
	/** The agent exited: parse its answer and write the docs. */
	void Complete();

	FProtoBoyDocMap& Map;
	FProtoBoyAgentRun Agent;
	bool bFinished = false;
	bool bSucceeded = false;
};
