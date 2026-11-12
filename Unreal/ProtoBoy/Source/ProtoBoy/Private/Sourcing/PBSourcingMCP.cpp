#include "Sourcing/PBSourcingMCP.h"

#include "MCP/PBMCPToolHelpers.h"
#include "Sourcing/Fab/PBFabSource.h"

#define LOCTEXT_NAMESPACE "ProtoBoySourcingMCP"

namespace ProtoBoySourcingMCP
{
	void FabSearch(const FProtoBoyMCPToolCall& Call, FProtoBoyMCPToolDone Done)
	{
		if (!ProtoBoyMCPTools::Require(Call, Done, { TEXT("query") }))
		{
			return;
		}

		ProtoBoyFab::FSearch Search;
		Search.Query = ProtoBoyMCPTools::GetString(Call, TEXT("query"));
		Search.ListingType = ProtoBoyMCPTools::GetString(Call, TEXT("listing_type"));
		Search.bFreeOnly = ProtoBoyMCPTools::GetBool(Call, TEXT("free_only"), false);

		if (!ProtoBoyMCPTools::GetBool(Call, TEXT("open"), true))
		{
			Done(FProtoBoyMCPToolResult::Ok(FString::Printf(TEXT("Fab search URL (not opened): %s"), *ProtoBoyFab::BuildSearchUrl(Search))));
			return;
		}

		FString Url;
		FText Error;
		if (!ProtoBoyFab::OpenSearch(Search, Url, Error))
		{
			Done(FProtoBoyMCPToolResult::Error(Error));
			return;
		}

		Done(FProtoBoyMCPToolResult::Ok(FString::Printf(TEXT(
			"Opened a Fab search in the user's browser: %s\n\n"
			"ProtoBoy can't see Fab results (Fab has no public search API). Tell the user what to look for (style, scale, "
			"poly budget, license) based on the design doc and art direction, and ask which listings they pick. "
			"Then record the choices (listing name and link, what each is for) in the relevant design doc with docs_update_section. "
			"Don't substitute AI-generated assets if nothing fits; use a marked placeholder and ask the user."), *Url)));
	}

	void RegisterTools(TArray<FProtoBoyMCPTool>& OutTools)
	{
		OutTools.Add(ProtoBoyMCPTools::MakeTool(TEXT("fab_search"), LOCTEXT("FabTitle", "Search Fab"),
			LOCTEXT("FabDesc", "Opens a Fab (fab.com) asset search in the user's browser, for assets that fit the design and art direction. You get the URL, not results: the user browses and picks. Use instead of generating assets."),
			false, {
				{ TEXT("query"), TEXT("string"), TEXT("Search terms derived from the design need and art style, e.g. \"stylized low poly medieval crates\"."), true },
				{ TEXT("listing_type"), TEXT("string"), TEXT("Optional Fab listing type filter, e.g. \"3d-model\", \"material\", \"audio\".") },
				{ TEXT("free_only"), TEXT("boolean"), TEXT("Only free listings.") },
				{ TEXT("open"), TEXT("boolean"), TEXT("Open it in the user's browser (default true). False returns the URL only.") },
			}, &FabSearch, &ProtoBoyFab::IsEnabled));
	}

	void RegisterPrompts(TArray<FProtoBoyMCPPrompt>& OutPrompts)
	{
		OutPrompts.Add({
			TEXT("source-assets"),
			TEXT("Source assets"),
			TEXT("Find assets for a need from the project's enabled sources (e.g. Fab) that fit the design and art direction, instead of generating them."),
			{ { TEXT("need"), TEXT("What the assets are for, e.g. \"props for the tavern level\"."), true } },
			[](const TMap<FString, FString>& Args)
			{
				return FString::Printf(TEXT(R"MD(Find assets for this need using ProtoBoy's asset sourcing. Don't generate assets with AI.

Need: %s

1. Call `get_project_context` and read its Asset sourcing section: the policy, the enabled sources, and the art
   direction doc. Read the art direction doc (`docs_read`) and the design doc for this feature, if there is one.
2. Write down what fits: style, scale, level of detail or poly budget, palette, mood, license, and anything to avoid.
   Check it against the art direction; if they conflict, ask me.
3. If Fab is enabled, call `fab_search` with search terms built from that (one search per distinct asset type). You
   only get the URL, not results: tell me what to look for on each search.
4. Ask which listings I pick. Record them in the design doc (`docs_update_section`, mode "append"): name, link, what
   each is for, license.
5. Where nothing fits, propose a clearly marked placeholder and a follow-up task (`task_create`), never an
   AI-generated asset.)MD"), *Args.FindRef(TEXT("need")).TrimStartAndEnd());
			}
		});
	}
}

#undef LOCTEXT_NAMESPACE
