#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"
#include "MCP/PBMCPConnection.h"

/**
 * Runs one of the user's agents unattended on a prompt, for ProtoBoy's generation windows (docs from the project,
 * tasks from the docs). ProtoBoy holds no LLM keys: it runs the agent's own CLI in the project folder (Claude Code with
 * `claude -p --output-format stream-json`, Gemini CLI with `gemini -p`), read-only, with the prompt file on stdin.
 * The agent answers with JSON; the owner parses it and writes the result, so the agent never writes files itself.
 *
 * Tick() is polled by the UI (generation windows are modal, so the core ticker doesn't run). Progress lines (tool use,
 * "PROGRESS: ..." lines) go to the log. The prompt and the agent's raw output are kept in the run's work folder.
 */
class PROTOBOY_API FProtoBoyAgentRun
{
public:
	struct FParams
	{
		EProtoBoyMCPClient Client = EProtoBoyMCPClient::ClaudeCode;
		FString Prompt;
		/** Gets Prompt.md and Output.log. */
		FString WorkDir;
		/** Logged once the agent has started. */
		FText StartedMessage;
		/** Logged when the agent starts writing its JSON answer. */
		FText AnswerMessage;
		/** Runs after the prompt is written and before the agent starts; false stops the start with its error. */
		TFunction<bool(FText& OutError)> BeforeLaunch;
		/** Let the agent search the web (Claude Code's WebSearch; never WebFetch). Off for every run that only needs the project. */
		bool bAllowWebSearch = false;
	};

	~FProtoBoyAgentRun();

	/** The agents this user has set up (MCP settings), in a stable order. */
	static TArray<EProtoBoyMCPClient> GetSetUpAgents();
	/** False with a reason for agents that can't run unattended (Claude Desktop). */
	static bool CanRun(EProtoBoyMCPClient Client, FText& OutWhyNot);
	/** The project folder the agent runs in, no trailing slash. */
	static FString GetProjectDir();

	/** Write the prompt and start the agent. False (nothing running) with a reason. Use a new run for each attempt. */
	bool Start(const FParams& Params, FText& OutError);
	/** Read the agent's output. True once, when the agent has exited (its output is saved by then). */
	bool Tick();
	/** Stop the agent if it's still running. */
	void Stop();

	bool IsRunning() const { return bRunning; }
	EProtoBoyMCPClient GetClient() const { return Client; }
	int32 GetReturnCode() const { return ReturnCode; }
	/** The JSON object in the agent's answer (the last ```json fence, else the outermost braces), or empty. */
	FString ExtractJson() const;
	/** Why there's no answer: the CLI isn't installed, or it stopped early (with where its output is). */
	FText DescribeNoAnswer() const;
	FString GetOutputLogPath() const;

	void AddLog(const FString& Line);
	/** Every progress line so far, oldest first. */
	const TArray<FString>& GetLog() const { return Log; }

private:
	/** One complete line of the agent's output. */
	void HandleLine(const FString& Line);
	void HandleClaudeEvent(const FString& Line);
	void HandleTextLine(const FString& Line);
	void ClosePipes();

	EProtoBoyMCPClient Client = EProtoBoyMCPClient::ClaudeCode;
	FString WorkDir;
	FText AnswerMessage;
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
	bool bRunning = false;
	int32 ReturnCode = 0;
	TArray<FString> Log;
};
