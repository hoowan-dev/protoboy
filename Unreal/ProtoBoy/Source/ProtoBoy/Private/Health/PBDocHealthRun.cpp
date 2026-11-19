#include "Health/PBDocHealthRun.h"

#include "Agents/PBAgentPromptContext.h"
#include "Docs/PBDocLayers.h"
#include "Docs/PBDocMap.h"
#include "Dom/JsonObject.h"
#include "Health/PBDocHealth.h"
#include "MCP/PBMCPClientConfig.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "PBSubsystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "ProtoBoyDocHealthRun"

namespace ProtoBoyDocHealthRun
{
	/** Per doc, so a big doc can't crowd out the others. */
	constexpr int32 MaxDocChars = 6000;
	/** Docs given to the agent; with more, the ones naming the most changed files go first. */
	constexpr int32 MaxDocs = 60;
	/** Changed files listed in the prompt (the newest). */
	constexpr int32 MaxListedFiles = 300;
	/** Named files listed under each doc. */
	constexpr int32 MaxNamedListed = 40;
	/** Files kept behind each stale doc. */
	constexpr int32 MaxFilesPerDoc = 8;

	FString FormatDate(const FDateTime& Utc)
	{
		return Utc.ToString(TEXT("%Y-%m-%d %H:%M"));
	}

	FString DescribeFiles(const FProtoBoyDocHealthChanges& Changes)
	{
		FString Out;
		for (int32 Index = 0; Index < Changes.Files.Num() && Index < MaxListedFiles; ++Index)
		{
			const FProtoBoyChangedFile& File = Changes.Files[Index];
			Out += FString::Printf(TEXT("- `%s`: %s"), *File.Path, *FormatDate(File.ChangedUtc));
			if (File.NumCommits > 0)
			{
				Out += FString::Printf(TEXT(", %d %s, last: \"%s\""), File.NumCommits, File.NumCommits == 1 ? TEXT("commit") : TEXT("commits"), *File.Subject.Left(120));
			}
			if (File.bUncommitted)
			{
				Out += TEXT(", has uncommitted changes");
			}
			Out += TEXT("\n");
		}
		if (Changes.Files.Num() > MaxListedFiles)
		{
			Out += FString::Printf(TEXT("- ...and %d older ones\n"), Changes.Files.Num() - MaxListedFiles);
		}
		return Out;
	}

	FString DescribeDocs(const FProtoBoyDocHealthChanges& Changes)
	{
		const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
		FString Out;
		for (const FProtoBoyDocChanges& Doc : Changes.Docs)
		{
			const FProtoBoyDocNode* Node = Subsystem ? Subsystem->GetDocMap().Find(Doc.DocId) : nullptr;
			if (!Node)
			{
				continue;
			}
			Out += FString::Printf(TEXT("### \"%s\" (%s doc, id `%s`, last edited %s)\n\n"), *Doc.Title,
				*ProtoBoyDocLayers::GetDisplayName(Doc.Layer).ToString(), *Doc.DocId, *FormatDate(Doc.ModifiedUtc));
			Out += FString::Printf(TEXT("%d of the changed files are newer than this doc."), Doc.NumChanged);
			if (Doc.Named.IsEmpty())
			{
				Out += TEXT(" Its text names none of them directly.\n\n");
			}
			else
			{
				Out += TEXT(" Its text names these (a file, class or folder name):\n\n");
				for (int32 Index = 0; Index < Doc.Named.Num() && Index < MaxNamedListed; ++Index)
				{
					Out += FString::Printf(TEXT("- `%s`\n"), *Changes.Files[Doc.Named[Index]].Path);
				}
				if (Doc.Named.Num() > MaxNamedListed)
				{
					Out += FString::Printf(TEXT("- ...and %d more\n"), Doc.Named.Num() - MaxNamedListed);
				}
				Out += TEXT("\n");
			}
			Out += FString::Printf(TEXT("<doc id=\"%s\">\n%s</doc>\n\n"), *Doc.DocId, *ProtoBoyAgentPrompt::ReadDoc(*Node, MaxDocChars));
		}
		return Out;
	}

	FString BuildPrompt(const FProtoBoyDocHealthChanges& Changes)
	{
		return FString::Printf(TEXT(R"MD(# Find the docs that may be out of date in the Unreal Engine project "%s"

The team keeps living docs for this project. Below are the docs, each with the date it was last edited, and the project
files that changed in revision control since then. Work out which docs may need updating because of those changes.

## Rules

- Read only. Don't create, edit or delete any file, and don't run commands.
- You may read the changed files (Source/, Plugins/, Config/) to see what they do now. .uasset and .umap files are
  binary: go by their names and folders.
- A doc may need updating when a change newer than it makes something it says wrong or incomplete: a class, asset or
  folder it names was renamed, moved or removed; behaviour, values or data it describes changed; or something new was
  added that the doc clearly should cover.
- A changed file isn't enough on its own. Formatting, comments and refactors that keep the behaviour the doc describes
  don't count, and neither do changes older than the doc's last edit.
- Design docs describe intent: flag one only when the project now contradicts what it says. Tech and Content docs
  describe what's there: flag them when what's there changed.
- The files a doc names are only a starting point. Check them first, then look for changed files the doc is about
  without naming them.
- As you work, print one short line per step that starts with "PROGRESS: ", e.g. "PROGRESS: Checking the Combat doc
  against Source/Game/Combat". Keep them short; they're shown to the user as a progress log.
- Leave out the docs that look fine. An empty "docs" list is a good answer when nothing needs updating.

## Changed files (newest first, dates in UTC)

%s
## The docs to check (dates in UTC)

%s## Your answer

When you're done, reply with only this JSON object inside a ```json fence, and nothing after it:

```json
{
  "docs": [
    {
      "id": "the doc's id, exactly as given above",
      "confidence": "high",
      "reason": "One or two sentences: what in the doc looks out of date, and which change did it.",
      "files": ["Source/Game/Hero.cpp"]
    }
  ]
}
```

"confidence" is "high" (the doc says something that's now wrong), "medium" (it's probably incomplete or partly wrong)
or "low" (worth a look). "files" are the changed files behind your judgement, most important first, at most %d, with
their paths exactly as listed above.
)MD"), FApp::GetProjectName(), *DescribeFiles(Changes), *DescribeDocs(Changes), MaxFilesPerDoc);
	}

	/** high, medium, low: 0, 1, 2. Anything else the agent writes counts as medium. */
	int32 GetConfidenceRank(const FString& Confidence)
	{
		return Confidence == TEXT("high") ? 0 : Confidence == TEXT("low") ? 2 : 1;
	}
}

FProtoBoyDocHealthRun::~FProtoBoyDocHealthRun()
{
	Agent.Stop();
}

FString FProtoBoyDocHealthRun::GetWorkDir()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ProtoBoy") / TEXT("DocHealth"));
}

bool FProtoBoyDocHealthRun::Start(EProtoBoyMCPClient InClient, FText& OutError)
{
	if (IsRunning() || bFinished)
	{
		return false;
	}
	if (!ProtoBoyDocHealth::IsEnabled())
	{
		OutError = LOCTEXT("NotEnabled", "Health checks are off, or the editor isn't connected to revision control.");
		return false;
	}
	if (!FProtoBoyAgentRun::CanRun(InClient, OutError) || !ProtoBoyDocHealthChanges::Collect(Changes, OutError))
	{
		return false;
	}
	Client = InClient;
	Report.NumDocsChecked = Changes.NumDocsChecked;
	Report.NumChangedFiles = Changes.Files.Num();
	Agent.AddLog(FText::Format(LOCTEXT("LogCollected", "{0} project {0}|plural(one=file,other=files) changed since the oldest doc was edited; {1} of {2} {2}|plural(one=doc,other=docs) {1}|plural(one=is,other=are) older than a change."),
		Changes.Files.Num(), Changes.Docs.Num(), Changes.NumDocsChecked).ToString());

	// Nothing is newer than any doc: no need for an agent.
	if (Changes.Docs.IsEmpty())
	{
		Agent.AddLog(LOCTEXT("LogNothingChanged", "Nothing changed since the docs were last edited, so there's nothing for the agent to check.").ToString());
		Finish();
		return true;
	}
	if (Changes.Docs.Num() > ProtoBoyDocHealthRun::MaxDocs)
	{
		Agent.AddLog(FText::Format(LOCTEXT("LogTooManyDocs", "Checking only the {0} docs that name the most changed files."), FText::AsNumber(ProtoBoyDocHealthRun::MaxDocs)).ToString());
		Changes.Docs.StableSort([](const FProtoBoyDocChanges& A, const FProtoBoyDocChanges& B) { return A.Named.Num() > B.Named.Num(); });
		Changes.Docs.SetNum(ProtoBoyDocHealthRun::MaxDocs);
	}

	FProtoBoyAgentRun::FParams Params;
	Params.Client = Client;
	Params.Prompt = ProtoBoyDocHealthRun::BuildPrompt(Changes);
	Params.WorkDir = GetWorkDir();
	Params.StartedMessage = FText::Format(LOCTEXT("LogStarted", "Started {0}. It compares the docs with the changed files; this can take a few minutes."),
		FProtoBoyMCPClientConfig::GetClientName(Client));
	Params.AnswerMessage = LOCTEXT("LogWriting", "The agent is writing its findings...");
	return Agent.Start(Params, OutError);
}

void FProtoBoyDocHealthRun::Tick()
{
	if (!bFinished && Agent.Tick())
	{
		Complete();
	}
}

void FProtoBoyDocHealthRun::Complete()
{
	const FString Json = Agent.ExtractJson();
	TSharedPtr<FJsonObject> Root;
	const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
	if (Json.IsEmpty())
	{
		Agent.AddLog(Agent.DescribeNoAnswer().ToString());
	}
	else if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid() || !Root->TryGetArrayField(TEXT("docs"), Array))
	{
		Agent.AddLog(FText::Format(LOCTEXT("LogBadJson", "The agent's answer isn't the expected JSON. Its output is in {0}."), FText::FromString(Agent.GetOutputLogPath())).ToString());
		Array = nullptr;
	}

	// No answer: the docs that name changed files are still worth a look.
	if (!Array)
	{
		Report.bAgentAnswered = false;
		for (const FProtoBoyDocChanges& Doc : Changes.Docs)
		{
			if (Doc.Named.IsEmpty())
			{
				continue;
			}
			FProtoBoyStaleDoc Stale;
			Stale.DocId = Doc.DocId;
			Stale.Path = Doc.Path;
			Stale.Title = Doc.Title;
			Stale.DocModifiedUtc = Doc.ModifiedUtc;
			Stale.Reason = LOCTEXT("FallbackReason", "This doc names files that changed after it was last edited. The agent didn't judge whether that matters.").ToString();
			for (int32 Index = 0; Index < Doc.Named.Num() && Index < ProtoBoyDocHealthRun::MaxFilesPerDoc; ++Index)
			{
				Stale.Files.Add(Changes.Files[Doc.Named[Index]].Path);
			}
			Report.Docs.Add(MoveTemp(Stale));
		}
		Agent.AddLog(FText::Format(LOCTEXT("LogFallback", "Listing the {0} {0}|plural(one=doc,other=docs) that {0}|plural(one=names,other=name) changed files instead."), Report.Docs.Num()).ToString());
		Finish();
		return;
	}

	TSet<FString> SeenIds;
	for (const TSharedPtr<FJsonValue>& Value : *Array)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		FString Id;
		if (!Value->TryGetObject(Object) || !(*Object)->TryGetStringField(TEXT("id"), Id))
		{
			continue;
		}
		Id = Id.TrimStartAndEnd();
		// Only docs the agent was given, each once.
		const FProtoBoyDocChanges* Doc = Changes.Docs.FindByPredicate([&Id](const FProtoBoyDocChanges& Candidate) { return Candidate.DocId == Id; });
		if (!Doc || SeenIds.Contains(Id))
		{
			continue;
		}
		SeenIds.Add(Id);

		FProtoBoyStaleDoc Stale;
		Stale.DocId = Doc->DocId;
		Stale.Path = Doc->Path;
		Stale.Title = Doc->Title;
		Stale.DocModifiedUtc = Doc->ModifiedUtc;
		(*Object)->TryGetStringField(TEXT("confidence"), Stale.Confidence);
		(*Object)->TryGetStringField(TEXT("reason"), Stale.Reason);
		Stale.Confidence = Stale.Confidence.TrimStartAndEnd().ToLower();
		if (Stale.Confidence != TEXT("high") && Stale.Confidence != TEXT("low"))
		{
			Stale.Confidence = TEXT("medium");
		}
		Stale.Reason = Stale.Reason.TrimStartAndEnd();

		// Only files that really changed after this doc, spelled as revision control has them.
		TArray<FString> Files;
		(*Object)->TryGetStringArrayField(TEXT("files"), Files);
		for (const FString& Given : Files)
		{
			const FString Path = Given.TrimStartAndEnd().Replace(TEXT("\\"), TEXT("/"));
			for (int32 Index = 0; Index < Doc->NumChanged; ++Index)
			{
				if (Changes.Files[Index].Path.Equals(Path, ESearchCase::IgnoreCase))
				{
					if (Stale.Files.Num() < ProtoBoyDocHealthRun::MaxFilesPerDoc)
					{
						Stale.Files.AddUnique(Changes.Files[Index].Path);
					}
					break;
				}
			}
		}
		Report.Docs.Add(MoveTemp(Stale));
	}
	Report.Docs.StableSort([](const FProtoBoyStaleDoc& A, const FProtoBoyStaleDoc& B)
	{
		return ProtoBoyDocHealthRun::GetConfidenceRank(A.Confidence) < ProtoBoyDocHealthRun::GetConfidenceRank(B.Confidence);
	});
	Agent.AddLog(Report.Docs.IsEmpty()
		? LOCTEXT("LogNoneStale", "Done: the agent found no docs that need updating.").ToString()
		: FText::Format(LOCTEXT("LogDone", "Done: {0} {0}|plural(one=doc,other=docs) may need updating."), Report.Docs.Num()).ToString());
	Finish();
}

void FProtoBoyDocHealthRun::Finish()
{
	Report.CheckedUtc = FDateTime::UtcNow();
	// No agent ran when nothing had changed.
	Report.AgentName = Changes.Docs.IsEmpty() ? FString() : FProtoBoyMCPClientConfig::GetClientName(Client).ToString();
	ProtoBoyDocHealthReport::Save(Report);
	bFinished = true;
	bSucceeded = true;
}

void FProtoBoyDocHealthRun::Cancel()
{
	if (bFinished)
	{
		return;
	}
	const bool bWasRunning = IsRunning();
	bCancelled = true;
	Agent.Stop();
	bFinished = true;
	if (bWasRunning)
	{
		Agent.AddLog(LOCTEXT("LogCancelled", "Cancelled.").ToString());
	}
}

#undef LOCTEXT_NAMESPACE
