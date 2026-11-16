#include "Tasks/PBTasksMCP.h"

#include "Docs/PBDocMap.h"
#include "MCP/PBMCPToolHelpers.h"
#include "PBSubsystem.h"
#include "Tasks/PBTaskProvider.h"

#define LOCTEXT_NAMESPACE "ProtoBoyTasksMCP"

namespace ProtoBoyTasksMCP
{
	using ProtoBoyMCPTools::FParam;
	using ProtoBoyMCPTools::GetBool;
	using ProtoBoyMCPTools::GetInt;
	using ProtoBoyMCPTools::GetString;
	using ProtoBoyMCPTools::GetSubsystemOrFail;
	using ProtoBoyMCPTools::MakeTool;
	using ProtoBoyMCPTools::Require;

	/** The task board, or null after finishing the call with the reason it can't be used. */
	IProtoBoyTaskProvider* GetTasksOrFail(const FProtoBoyMCPToolDone& Done)
	{
		UProtoBoySubsystem* Subsystem = GetSubsystemOrFail(Done);
		if (!Subsystem)
		{
			return nullptr;
		}
		IProtoBoyTaskProvider* Tasks = Subsystem->GetTaskProvider();
		FText WhyNot = LOCTEXT("NoBoard", "No task board is connected. Ask the user to connect one in ProtoBoy > Connect Task Board.");
		if (!Tasks || !Tasks->IsReady(WhyNot))
		{
			Done(FProtoBoyMCPToolResult::Error(WhyNot.ToString() + TEXT(" (ProtoBoy > Connect Task Board)")));
			return nullptr;
		}
		return Tasks;
	}


	FString StatusLabel(const FProtoBoyTask& Task)
	{
		const FString Status = ProtoBoyTasks::StatusToString(Task.Status);
		return Task.StatusName.IsEmpty() ? Status : FString::Printf(TEXT("%s, list \"%s\""), *Status, *Task.StatusName);
	}

	FString FormatTaskLine(const FProtoBoyTask& Task)
	{
		FString Line = FString::Printf(TEXT("- [%s] %s (%s)"), *Task.Id, *Task.Title, *StatusLabel(Task));
		if (!Task.Labels.IsEmpty())
		{
			Line += FString::Printf(TEXT(" labels: %s"), *FString::Join(Task.Labels, TEXT(", ")));
		}
		if (!Task.DesignDoc.IsEmpty())
		{
			Line += FString::Printf(TEXT(" design doc: %s"), *Task.DesignDoc);
		}
		return Line;
	}

	FString FormatTask(const FProtoBoyTask& Task)
	{
		FString Out = FString::Printf(TEXT("# %s\n\n- **Id:** %s\n- **Status:** %s\n- **Url:** %s\n"),
			*Task.Title, *Task.Id, *StatusLabel(Task), *Task.Url);
		if (!Task.Labels.IsEmpty())
		{
			Out += FString::Printf(TEXT("- **Labels:** %s\n"), *FString::Join(Task.Labels, TEXT(", ")));
		}
		if (!Task.DueUtc.IsEmpty())
		{
			Out += FString::Printf(TEXT("- **Due:** %s\n"), *Task.DueUtc);
		}
		Out += FString::Printf(TEXT("- **Design doc:** %s\n"), Task.DesignDoc.IsEmpty()
			? TEXT("none linked (link one with a \"Design doc: <path>\" line, or ask the user which doc applies)")
			: *Task.DesignDoc);
		Out += FString::Printf(TEXT("\n## Description\n\n%s\n"), Task.Description.IsEmpty() ? TEXT("_No description._") : *Task.Description);
		if (!Task.Comments.IsEmpty())
		{
			Out += TEXT("\n## Comments (newest first)\n\n");
			for (const FProtoBoyTaskComment& Comment : Task.Comments)
			{
				Out += FString::Printf(TEXT("- **%s** (%s): %s\n"), *Comment.Author, *Comment.DateUtc, *Comment.Text.Replace(TEXT("\n"), TEXT(" ")));
			}
		}
		return Out;
	}

	// -----------------------------------------------------------------------------------------------------------------
	// Tasks

	void TasksList(const FProtoBoyMCPToolCall& Call, FProtoBoyMCPToolDone Done)
	{
		IProtoBoyTaskProvider* Tasks = GetTasksOrFail(Done);
		if (!Tasks)
		{
			return;
		}

		const FString StatusArg = GetString(Call, TEXT("status"));
		TOptional<EProtoBoyTaskStatus> Filter;
		if (!StatusArg.IsEmpty() && StatusArg != TEXT("all"))
		{
			Filter = ProtoBoyTasks::StatusFromString(StatusArg);
			if (!Filter.IsSet())
			{
				Done(FProtoBoyMCPToolResult::Error(TEXT("status must be todo, in_progress, done or all.")));
				return;
			}
		}

		const FString BoardName = Tasks->GetDisplayName().ToString();
		Tasks->ListTasks(Filter, [BoardName, Done = MoveTemp(Done)](const TProtoBoyTaskResult<TArray<FProtoBoyTask>>& Result)
		{
			if (!Result.bOk)
			{
				Done(FProtoBoyMCPToolResult::Error(Result.Error));
				return;
			}
			FString Out = FString::Printf(TEXT("%d open tasks on %s:\n\n"), Result.Value.Num(), *BoardName);
			for (const FProtoBoyTask& Task : Result.Value)
			{
				Out += FormatTaskLine(Task) + TEXT("\n");
			}
			Done(FProtoBoyMCPToolResult::Ok(Out));
		});
	}

	void TaskGet(const FProtoBoyMCPToolCall& Call, FProtoBoyMCPToolDone Done)
	{
		IProtoBoyTaskProvider* Tasks = GetTasksOrFail(Done);
		if (!Tasks || !Require(Call, Done, { TEXT("id") }))
		{
			return;
		}

		Tasks->GetTask(GetString(Call, TEXT("id")), [Done = MoveTemp(Done)](const TProtoBoyTaskResult<FProtoBoyTask>& Result)
		{
			Done(Result.bOk ? FProtoBoyMCPToolResult::Ok(FormatTask(Result.Value)) : FProtoBoyMCPToolResult::Error(Result.Error));
		});
	}

	void TaskCreate(const FProtoBoyMCPToolCall& Call, FProtoBoyMCPToolDone Done)
	{
		IProtoBoyTaskProvider* Tasks = GetTasksOrFail(Done);
		if (!Tasks || !Require(Call, Done, { TEXT("title") }))
		{
			return;
		}

		const FString StatusArg = GetString(Call, TEXT("status"));
		const TOptional<EProtoBoyTaskStatus> Status = StatusArg.IsEmpty() ? TOptional<EProtoBoyTaskStatus>(EProtoBoyTaskStatus::ToDo) : ProtoBoyTasks::StatusFromString(StatusArg);
		if (!Status.IsSet())
		{
			Done(FProtoBoyMCPToolResult::Error(TEXT("status must be todo, in_progress or done.")));
			return;
		}

		FString Description = GetString(Call, TEXT("description"));
		const FString DesignDoc = GetString(Call, TEXT("design_doc"));
		if (!DesignDoc.IsEmpty())
		{
			Description += FString::Printf(TEXT("\n\n%s"), *ProtoBoyTasks::MakeDesignDocLine(DesignDoc));
		}
		Description += FString::Printf(TEXT("\n\n_Created by %s via ProtoBoy._"), *Call.ClientName);

		// In a task category: its To Do list, tagged with its label.
		const FString Category = GetString(Call, TEXT("category"));
		if (!Category.IsEmpty())
		{
			if (Status.GetValue() != EProtoBoyTaskStatus::ToDo)
			{
				Done(FProtoBoyMCPToolResult::Error(TEXT("A task with a category starts in its To Do list: leave status out (or \"todo\").")));
				return;
			}
			// The category's doc, so the task goes in its list even if it was renamed since the last Sync.
			FProtoBoyNewTask New;
			New.Title = GetString(Call, TEXT("title"));
			New.Description = Description.TrimStart();
			if (const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get())
			{
				for (const FProtoBoyDocNode* Doc : Subsystem->GetDocMap().GetTaskCategories())
				{
					if (Doc->Title.Equals(Category.TrimStartAndEnd(), ESearchCase::IgnoreCase))
					{
						New.CategoryId = Doc->Id;
						New.Category = Doc->Title;
						break;
					}
				}
			}
			const TSharedRef<FProtoBoyTask> Created = MakeShared<FProtoBoyTask>();
			Tasks->CreateTasks({ New },
				[Created](int32, const TProtoBoyTaskResult<FProtoBoyTask>& Result)
				{
					if (Result.bOk)
					{
						*Created = Result.Value;
					}
					return true;
				},
				[Created, Done = MoveTemp(Done)](const TProtoBoyTaskResult<bool>& Result)
				{
					Done(Result.bOk
						? FProtoBoyMCPToolResult::Ok(FString::Printf(TEXT("Created task [%s] %s in \"%s\" (%s)."), *Created->Id, *Created->Title, *Created->StatusName, *Created->Url))
						: FProtoBoyMCPToolResult::Error(Result.Error));
				});
			return;
		}

		Tasks->CreateTask(GetString(Call, TEXT("title")), Description.TrimStart(), Status.GetValue(),
			[Done = MoveTemp(Done)](const TProtoBoyTaskResult<FProtoBoyTask>& Result)
			{
				Done(Result.bOk
					? FProtoBoyMCPToolResult::Ok(FString::Printf(TEXT("Created task [%s] %s (%s)."), *Result.Value.Id, *Result.Value.Title, *Result.Value.Url))
					: FProtoBoyMCPToolResult::Error(Result.Error));
			});
	}

	void TaskUpdateStatus(const FProtoBoyMCPToolCall& Call, FProtoBoyMCPToolDone Done)
	{
		IProtoBoyTaskProvider* Tasks = GetTasksOrFail(Done);
		if (!Tasks || !Require(Call, Done, { TEXT("id"), TEXT("status") }))
		{
			return;
		}

		const TOptional<EProtoBoyTaskStatus> Status = ProtoBoyTasks::StatusFromString(GetString(Call, TEXT("status")));
		if (!Status.IsSet())
		{
			Done(FProtoBoyMCPToolResult::Error(TEXT("status must be todo, in_progress or done.")));
			return;
		}

		Tasks->SetTaskStatus(GetString(Call, TEXT("id")), Status.GetValue(), [Done = MoveTemp(Done)](const TProtoBoyTaskResult<FProtoBoyTask>& Result)
		{
			Done(Result.bOk
				? FProtoBoyMCPToolResult::Ok(FString::Printf(TEXT("[%s] %s is now %s."), *Result.Value.Id, *Result.Value.Title, *ProtoBoyTasks::StatusToString(Result.Value.Status)))
				: FProtoBoyMCPToolResult::Error(Result.Error));
		});
	}

	void TaskComment(const FProtoBoyMCPToolCall& Call, FProtoBoyMCPToolDone Done)
	{
		IProtoBoyTaskProvider* Tasks = GetTasksOrFail(Done);
		if (!Tasks || !Require(Call, Done, { TEXT("id"), TEXT("text") }))
		{
			return;
		}

		const FString Text = FString::Printf(TEXT("%s\n\n_%s via ProtoBoy_"), *GetString(Call, TEXT("text")), *Call.ClientName);
		Tasks->AddComment(GetString(Call, TEXT("id")), Text, [Done = MoveTemp(Done)](const TProtoBoyTaskResult<bool>& Result)
		{
			Done(Result.bOk ? FProtoBoyMCPToolResult::Ok(TEXT("Comment added.")) : FProtoBoyMCPToolResult::Error(Result.Error));
		});
	}

	// -----------------------------------------------------------------------------------------------------------------
	// Registration

	void RegisterTools(TArray<FProtoBoyMCPTool>& OutTools)
	{
		const TArray<FString> StatusValues = { TEXT("todo"), TEXT("in_progress"), TEXT("done") };
		const TArray<FString> ListStatusValues = { TEXT("todo"), TEXT("in_progress"), TEXT("done"), TEXT("all") };

		auto Add = [&OutTools](const TCHAR* Name, const FText& Title, const FText& Description, bool bWrites,
			const TArray<FParam>& Params, FProtoBoyMCPToolHandler Handler)
		{
			OutTools.Add(MakeTool(Name, Title, Description, bWrites, Params, MoveTemp(Handler)));
		};

		Add(TEXT("tasks_list"), LOCTEXT("TasksTitle", "List tasks"),
			LOCTEXT("TasksDesc", "Lists open tasks on the project's task board."),
			false, { { TEXT("status"), TEXT("string"), TEXT("Filter by status (default all)."), false, ListStatusValues } }, &TasksList);
		Add(TEXT("task_get"), LOCTEXT("TaskGetTitle", "Get task"),
			LOCTEXT("TaskGetDesc", "Gets a task's description, status, linked design doc and recent comments."),
			false, { { TEXT("id"), TEXT("string"), TEXT("Task id from tasks_list."), true } }, &TaskGet);
		Add(TEXT("task_create"), LOCTEXT("TaskCreateTitle", "Create task"),
			LOCTEXT("TaskCreateDesc", "Creates a task on the board. Link the design doc it implements."),
			true, {
				{ TEXT("title"), TEXT("string"), TEXT("Task title."), true },
				{ TEXT("description"), TEXT("string"), TEXT("What to do and the acceptance criteria.") },
				{ TEXT("status"), TEXT("string"), TEXT("Initial status (default todo)."), false, StatusValues },
				{ TEXT("design_doc"), TEXT("string"), TEXT("Design doc path this task implements, e.g. \"Design/Inventory.md\".") },
				{ TEXT("category"), TEXT("string"), TEXT("Task category (a doc marked as one on the doc map; get_project_context lists them). The task goes in its \"To Do - <Category>\" list with its label. Unknown categories go to Uncategorized.") },
			}, &TaskCreate);
		Add(TEXT("task_update_status"), LOCTEXT("TaskStatusTitle", "Update task status"),
			LOCTEXT("TaskStatusDesc", "Moves a task to todo, in_progress or done."),
			true, {
				{ TEXT("id"), TEXT("string"), TEXT("Task id."), true },
				{ TEXT("status"), TEXT("string"), TEXT("New status."), true, StatusValues },
			}, &TaskUpdateStatus);
		Add(TEXT("task_comment"), LOCTEXT("TaskCommentTitle", "Comment on task"),
			LOCTEXT("TaskCommentDesc", "Adds a progress comment to a task (what changed, follow-ups)."),
			true, {
				{ TEXT("id"), TEXT("string"), TEXT("Task id."), true },
				{ TEXT("text"), TEXT("string"), TEXT("Comment text (Markdown)."), true },
			}, &TaskComment);
	}

	void RegisterPrompts(TArray<FProtoBoyMCPPrompt>& OutPrompts)
	{
		OutPrompts.Add({
			TEXT("generate-tasks"),
			TEXT("Generate tasks"),
			TEXT("Draft task ideas for a task category from its docs (and the docs below it), talk them through, then add the approved ones to its To Do list."),
			{
				{ TEXT("category"), TEXT("The task category (a doc marked as one on the doc map). Leave empty to pick together."), false },
				{ TEXT("context"), TEXT("Anything extra to take into account (optional)."), false },
			},
			[](const TMap<FString, FString>& Args)
			{
				return FString::Printf(TEXT(R"MD(Draft tasks for a task category using ProtoBoy. Don't write code.

Category: %s
Extra context: %s

1. Call `get_project_context`. Its "Task categories" section lists the categories; if none is given above, or it isn't
   one of them, ask me which to use.
2. `docs_read` the category's doc and the docs below it on the doc map.
3. Call `tasks_list` (status "todo") so you don't repeat tasks already on the board.
4. Look at the code to see what's already built, so you suggest what's missing.
5. Suggest tasks small enough for one session each, each with what to do and acceptance criteria, all serving what the
   docs describe. Don't invent features the docs don't mention. Show them to me and wait.
6. After I approve, create each with `task_create`, passing the category (it goes in "To Do - <Category>" with the
   category's label) and the category's doc as `design_doc`.
7. Summarize what you created.)MD"), *ProtoBoyMCPTools::ArgOr(Args, TEXT("category"), TEXT("(not given)")), *ProtoBoyMCPTools::ArgOr(Args, TEXT("context"), TEXT("(none)")));
			}
		});

		OutPrompts.Add({
			TEXT("implement-task"),
			TEXT("Implement a task"),
			TEXT("Implement a task from the board as its Design and Tech docs describe, then update the task and docs."),
			{ { TEXT("task_id"), TEXT("The task's id (Trello card short link or id)."), true } },
			[](const TMap<FString, FString>& Args)
			{
				return FString::Printf(TEXT(R"MD(Implement task %s using ProtoBoy to build what the project's docs describe.

1. Call `task_get` for the task's description, comments and linked Design doc.
2. Call `get_project_context`, then `docs_read` the linked Design doc and the Tech docs under it.
   Use `locate_feature` if the task links no doc.
3. Plan where the code goes from the Tech docs and their Decisions. Don't add a new feature, system or module the docs
   don't cover without my agreement and a `record_decision` entry. Tell me the plan before large changes.
4. Move the task to in progress with `task_update_status`.
5. Implement in small steps inside the owning system. Keep to the task's scope; put anything extra into new tasks
   (`task_create`) instead of growing this one.
6. When finished:
   - Record how it works in the Tech doc, assets used in the Content doc, and any change to the design in the Design doc
     (`docs_update_section`, mode "append").
   - Comment on the task with a summary of the changes and follow-ups (`task_comment`).
   - Move it to done with `task_update_status` only if the acceptance criteria are met; otherwise leave it in progress
     and say what's left.)MD"), *ProtoBoyMCPTools::ArgOr(Args, TEXT("task_id"), TEXT("")));
			}
		});
	}
}

#undef LOCTEXT_NAMESPACE
