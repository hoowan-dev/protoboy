#include "Docs/PBDocLayers.h"

#define LOCTEXT_NAMESPACE "ProtoBoyDocLayers"

namespace ProtoBoyDocLayers
{
	const TCHAR* IdKey = TEXT("protoboy-id");
	const TCHAR* LayerKey = TEXT("protoboy-layer");
	const TCHAR* ParentKey = TEXT("protoboy-parent");
	const TCHAR* ImplementsKey = TEXT("protoboy-implements");

	TArray<EProtoBoyDocLayer> GetAllLayers()
	{
		return { EProtoBoyDocLayer::Vision, EProtoBoyDocLayer::Overview, EProtoBoyDocLayer::Design, EProtoBoyDocLayer::Tech,
			EProtoBoyDocLayer::Systems, EProtoBoyDocLayer::Content, EProtoBoyDocLayer::Direction, EProtoBoyDocLayer::Decision };
	}

	TArray<EProtoBoyDocLayer> GetAddableLayers()
	{
		return { EProtoBoyDocLayer::Design, EProtoBoyDocLayer::Systems, EProtoBoyDocLayer::Content, EProtoBoyDocLayer::Direction };
	}

	FString ToKey(EProtoBoyDocLayer Layer)
	{
		switch (Layer)
		{
		case EProtoBoyDocLayer::Vision: return TEXT("vision");
		case EProtoBoyDocLayer::Overview: return TEXT("overview");
		case EProtoBoyDocLayer::Design: return TEXT("design");
		case EProtoBoyDocLayer::Tech: return TEXT("tech");
		case EProtoBoyDocLayer::Systems: return TEXT("systems");
		case EProtoBoyDocLayer::Content: return TEXT("content");
		case EProtoBoyDocLayer::Direction: return TEXT("direction");
		case EProtoBoyDocLayer::Decision: return TEXT("decision");
		}
		return FString();
	}

	bool FromKey(const FString& Key, EProtoBoyDocLayer& OutLayer)
	{
		const FString Trimmed = Key.TrimStartAndEnd();
		for (const EProtoBoyDocLayer Layer : GetAllLayers())
		{
			if (Trimmed.Equals(ToKey(Layer), ESearchCase::IgnoreCase))
			{
				OutLayer = Layer;
				return true;
			}
		}
		return false;
	}

	FText GetDisplayName(EProtoBoyDocLayer Layer)
	{
		switch (Layer)
		{
		case EProtoBoyDocLayer::Vision: return LOCTEXT("Vision", "Vision");
		case EProtoBoyDocLayer::Overview: return LOCTEXT("Overview", "Overview");
		case EProtoBoyDocLayer::Design: return LOCTEXT("Design", "Design");
		case EProtoBoyDocLayer::Tech: return LOCTEXT("Tech", "Tech");
		case EProtoBoyDocLayer::Systems: return LOCTEXT("Systems", "Systems");
		case EProtoBoyDocLayer::Content: return LOCTEXT("Content", "Content");
		case EProtoBoyDocLayer::Direction: return LOCTEXT("Direction", "Direction");
		case EProtoBoyDocLayer::Decision: return LOCTEXT("Decision", "Decision");
		}
		return FText::GetEmpty();
	}

	FText GetDescription(EProtoBoyDocLayer Layer)
	{
		switch (Layer)
		{
		case EProtoBoyDocLayer::Vision: return LOCTEXT("VisionTip", "Why the game exists: the pitch, the design pillars every feature should serve, and the non-goals. One doc; rarely changes.");
		case EProtoBoyDocLayer::Overview: return LOCTEXT("OverviewTip", "What the game is, as a whole: summary, core loop, game flow, and an index of every doc below it. One doc.");
		case EProtoBoyDocLayer::Design: return LOCTEXT("DesignTip", "What a feature is, from the player's side: purpose, experience, rules, what the player sees. Nest design docs for sub-features.");
		case EProtoBoyDocLayer::Tech: return LOCTEXT("TechTip", "How a feature is built. Pairs with its Design doc and answers each of its sections from the engine side.");
		case EProtoBoyDocLayer::Systems: return LOCTEXT("SystemsTip", "Shared tech with no player face, e.g. save/load, input, networking: responsibilities, interface, data, who uses it.");
		case EProtoBoyDocLayer::Content: return LOCTEXT("ContentTip", "What exists in the game, e.g. levels, items, quests, characters: an inventory, authoring rules, where the data lives.");
		case EProtoBoyDocLayer::Direction: return LOCTEXT("DirectionTip", "How the game looks, sounds and reads, e.g. art, audio, UX, writing style: goals, style, references, budgets.");
		case EProtoBoyDocLayer::Decision: return LOCTEXT("DecisionTip", "Why something is the way it is: an architecture decision record (ADR).");
		}
		return FText::GetEmpty();
	}

	FString GetFolder(EProtoBoyDocLayer Layer)
	{
		switch (Layer)
		{
		case EProtoBoyDocLayer::Design: return TEXT("Design");
		case EProtoBoyDocLayer::Tech: return TEXT("Design");
		case EProtoBoyDocLayer::Systems: return TEXT("Systems");
		case EProtoBoyDocLayer::Content: return TEXT("Content");
		case EProtoBoyDocLayer::Direction: return TEXT("Direction");
		case EProtoBoyDocLayer::Decision: return TEXT("ADR");
		default: return FString();
		}
	}

	TArray<FHeading> GetHeadings(EProtoBoyDocLayer Layer)
	{
		switch (Layer)
		{
		case EProtoBoyDocLayer::Vision:
			return {
				{ TEXT("Pitch"), TEXT("One sentence: what the player does, and why it's fun.") },
				{ TEXT("Player Fantasy"), TEXT("Who the player gets to be, and how it should feel.") },
				{ TEXT("Design Pillars"), TEXT("3-5 experience goals. Every feature should serve at least one. One ### heading per pillar, with the feeling it gives and what it rules out. Replace the example.") },
				{ TEXT("Audience and Platform"), TEXT("Who it's for, and where it ships.") },
				{ TEXT("References"), TEXT("Games, films, art and music it draws on, and what it takes from each.") },
				{ TEXT("Non-goals"), TEXT("What this game deliberately isn't, so nobody builds it by accident.") },
			};
		case EProtoBoyDocLayer::Overview:
			return {
				{ TEXT("Summary"), TEXT("What the game is, in a paragraph.") },
				{ TEXT("Core Loop"), TEXT("What the player does over and over, step by step.") },
				{ TEXT("Game Flow"), TEXT("How a session goes from start to finish: menus, progression, ending.") },
				{ TEXT("Areas"), TEXT("Every doc below this one. ProtoBoy keeps the list up to date; write anything else around it.") },
				{ TEXT("Glossary"), TEXT("Terms the team uses, so docs and agents mean the same thing by them.") },
			};
		case EProtoBoyDocLayer::Design:
			return {
				{ TEXT("Purpose"), TEXT("Which design pillars (in the Vision) this serves, and how.") },
				{ TEXT("Player Experience"), TEXT("As a player, I can ... to feel ...") },
				{ TEXT("Rules"), TEXT("How it works, from the player's side. No implementation details.") },
				{ TEXT("What the Player Sees"), TEXT("Feedback, UI and presentation.") },
				{ TEXT("Edge Cases"), TEXT("What happens when things go wrong or meet each other.") },
				{ TEXT("Open Questions"), TEXT("What isn't decided yet.") },
			};
		case EProtoBoyDocLayer::Tech:
			return GetHeadings(EProtoBoyDocLayer::Design);
		case EProtoBoyDocLayer::Systems:
			return {
				{ TEXT("Responsibilities"), TEXT("What this system owns, and what it doesn't.") },
				{ TEXT("Interface"), TEXT("How other code uses it: classes, functions, events.") },
				{ TEXT("Data"), TEXT("What it stores, and where.") },
				{ TEXT("Used By"), TEXT("Which features and systems depend on it.") },
			};
		case EProtoBoyDocLayer::Content:
			return {
				{ TEXT("Inventory"), TEXT("What exists: a table of every item, level, character, ...") },
				{ TEXT("Authoring Rules"), TEXT("How to make a new one, and what makes a good one.") },
				{ TEXT("Where the Data Lives"), TEXT("Data assets, DataTables and folders.") },
				{ TEXT("Fed Into"), TEXT("Which features use this content.") },
			};
		case EProtoBoyDocLayer::Direction:
			return {
				{ TEXT("Goals"), TEXT("What it should make the player feel.") },
				{ TEXT("Style"), TEXT("The rules that keep it consistent.") },
				{ TEXT("References"), TEXT("Images, tracks or examples, and what to take from each.") },
				{ TEXT("Budgets"), TEXT("Limits: polycounts, texture sizes, memory, word counts, ...") },
			};
		case EProtoBoyDocLayer::Decision:
			break;
		}
		return {};
	}

	FString GetTechSectionHint()
	{
		return TEXT("How the engine side answers the Design doc's section of the same name: classes and modules, data and tunables, interfaces to other docs, constraints.");
	}

	FString GetIntro(EProtoBoyDocLayer Layer)
	{
		switch (Layer)
		{
		case EProtoBoyDocLayer::Vision: return TEXT("Why this game exists. Every other doc serves this one. Humans own it: agents read it before anything else, and never edit it.");
		case EProtoBoyDocLayer::Overview: return TEXT("What the game is, as a whole, and where everything is documented. Humans own it; ProtoBoy keeps the Areas list current.");
		case EProtoBoyDocLayer::Design: return TEXT("What this feature is, from the player's side. How it's built goes in its Tech doc.");
		case EProtoBoyDocLayer::Systems: return TEXT("Shared tech with no player face of its own.");
		case EProtoBoyDocLayer::Content: return TEXT("What exists in the game, and how it's authored.");
		case EProtoBoyDocLayer::Direction: return TEXT("How the game looks, sounds or reads.");
		default: return FString();
		}
	}

	FString MakeBody(const FString& Title, const FString& Intro, const TArray<FHeading>& Headings)
	{
		FString Out = FString::Printf(TEXT("# %s\n"), *Title);
		if (!Intro.IsEmpty())
		{
			Out += FString::Printf(TEXT("\n> %s\n"), *Intro);
		}
		for (const FHeading& Heading : Headings)
		{
			Out += FString::Printf(TEXT("\n## %s\n\n"), *Heading.Title);
			if (!Heading.Hint.IsEmpty())
			{
				Out += FString::Printf(TEXT("<!-- %s -->\n"), *Heading.Hint);
			}
		}
		return Out;
	}
}

#undef LOCTEXT_NAMESPACE
