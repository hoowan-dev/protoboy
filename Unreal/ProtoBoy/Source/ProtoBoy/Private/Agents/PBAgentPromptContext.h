#pragma once

#include "CoreMinimal.h"

struct FProtoBoyDocNode;

/**
 * Project context that several agent prompts share (doc generation, task generation, asset search). The agent runs in
 * the project folder but can't read .uasset files, nor docs that live outside the project (e.g. in an Obsidian vault),
 * so ProtoBoy puts them in the prompt.
 */
namespace ProtoBoyAgentPrompt
{
	/** A Markdown summary of the Content folder (/Game) from the asset registry: counts by type and folder, maps, Blueprints, data. */
	FString DescribeProjectAssets();

	/** A doc's text for a prompt, without its frontmatter and ProtoBoy's hints, cut at MaxChars. */
	FString ReadDoc(const FProtoBoyDocNode& Doc, int32 MaxChars);
}
