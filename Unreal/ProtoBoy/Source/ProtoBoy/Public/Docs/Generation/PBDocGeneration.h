#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"
#include "MCP/PBMCPConnection.h"

class FProtoBoyDocMap;

/**
 * Generates the project's docs from what's already in it, with one of the user's agents (Set Up Docs from Template >
 * Generate from the Project). ProtoBoy holds no LLM keys: it runs the agent's own CLI in the project folder (Claude Code
 * with `claude -p`, Gemini CLI with `gemini -p`), read-only, with a prompt that includes a summary of the Content folder
 * from the asset registry (the agent can't read .uasset files) and asks for the doc set as JSON in the template format
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

	/** The agents this user has set up (MCP settings), in a stable order. */
	static TArray<EProtoBoyMCPClient> GetSetUpAgents();
	/** False with a reason for agents that can't run unattended (Claude Desktop). */
	static bool CanRun(EProtoBoyMCPClient Client, FText& OutWhyNot);

	/** Clear the docs, write the prompt and start the agent. False (nothing started) with a reason. */
	bool Start(EProtoBoyMCPClient Client, FText& OutError);
	/** Read the agent's output, and once it exits, write the docs it returned. Call often while running. */
	void Tick();
	/** Stop the agent (if it's still running) and clear the docs. */
	void Cancel();

	bool IsRunning() const { return bRunning; }
	bool IsFinished() const { return bFinished; }
	bool Succeeded() const { return bSucceeded; }

	/** Every progress line so far, oldest first. */
	const TArray<FString>& GetLog() const { return Log; }

	/** Saved/ProtoBoy/DocGeneration/. */
	static FString GetWorkDir();

private:
	void AddLog(const FString& Line);
	/** One complete line of the agent's output. */
	void HandleLine(const FString& Line);
	void HandleClaudeEvent(const FString& Line);
	void HandleTextLine(const FString& Line);
	/** The agent exited: parse its answer and write the docs. */
	void Complete(int32 ReturnCode);
	void StopProcess();

	FProtoBoyDocMap& Map;
	EProtoBoyMCPClient Client = EProtoBoyMCPClient::ClaudeCode;
	FProcHandle Process;
	void* ReadPipe = nullptr;
	void* WritePipe = nullptr;
	/** Bytes of a line that hasn't ended yet. */
	TArray<uint8> Pending;
	/** Everything the agent printed, for the raw output file. */
	FString RawOutput;
	/** The agent's last answer: the final result event (Claude Code), or all of its output (Gemini CLI). */
	FString FinalText;
	bool bInJsonFence = false;
	bool bAgentError = false;
	bool bRunning = false;
	bool bFinished = false;
	bool bSucceeded = false;
	TArray<FString> Log;
};
