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
		return { EProtoBoyDocLayer::Game, EProtoBoyDocLayer::Design, EProtoBoyDocLayer::Tech, EProtoBoyDocLayer::Content };
	}

	FString ToKey(EProtoBoyDocLayer Layer)
	{
		switch (Layer)
		{
		case EProtoBoyDocLayer::Game: return TEXT("game");
		case EProtoBoyDocLayer::Design: return TEXT("design");
		case EProtoBoyDocLayer::Tech: return TEXT("tech");
		case EProtoBoyDocLayer::Content: return TEXT("content");
		}
		return FString();
	}

	bool FromKey(const FString& Key, EProtoBoyDocLayer& OutLayer)
	{
		const FString Trimmed = Key.TrimStartAndEnd().ToLower();
		for (const EProtoBoyDocLayer Layer : GetAllLayers())
		{
			if (Trimmed == ToKey(Layer))
			{
				OutLayer = Layer;
				return true;
			}
		}
		// The eight-layer version's keys, so docs written then stay on the map without being rewritten.
		if (Trimmed == TEXT("vision") || Trimmed == TEXT("overview"))
		{
			OutLayer = EProtoBoyDocLayer::Game;
			return true;
		}
		if (Trimmed == TEXT("systems"))
		{
			OutLayer = EProtoBoyDocLayer::Tech;
			return true;
		}
		if (Trimmed == TEXT("direction"))
		{
			OutLayer = EProtoBoyDocLayer::Content;
			return true;
		}
		return false;
	}

	bool IsRetiredKey(const FString& Key)
	{
		return Key.TrimStartAndEnd().Equals(TEXT("decision"), ESearchCase::IgnoreCase);
	}

	FText GetDisplayName(EProtoBoyDocLayer Layer)
	{
		switch (Layer)
		{
		case EProtoBoyDocLayer::Game: return LOCTEXT("Game", "Game");
		case EProtoBoyDocLayer::Design: return LOCTEXT("Design", "Design");
		case EProtoBoyDocLayer::Tech: return LOCTEXT("Tech", "Tech");
		case EProtoBoyDocLayer::Content: return LOCTEXT("Content", "Content");
		}
		return FText::GetEmpty();
	}

	FText GetDescription(EProtoBoyDocLayer Layer)
	{
		switch (Layer)
		{
		case EProtoBoyDocLayer::Game: return LOCTEXT("GameTip", "Why the game exists and what it is as a whole: pitch, design pillars, non-goals, core loop, and an index of every other doc. One doc, the top of the map.");
		case EProtoBoyDocLayer::Design: return LOCTEXT("DesignTip", "What a feature is, from the player's side: purpose, experience, rules, what the player sees. Nest design docs for sub-features.");
		case EProtoBoyDocLayer::Tech: return LOCTEXT("TechTip", "How something is built: a feature's Tech doc answers each section of its Design doc from the engine side; shared tech (save/load, input, networking) stands on its own.");
		case EProtoBoyDocLayer::Content: return LOCTEXT("ContentTip", "What's in the game (levels, items, quests, characters) and how it looks, sounds and reads (art, audio, UX and writing style guides).");
		}
		return FText::GetEmpty();
	}

	EProtoBoyDocLayer GetLayer(EProtoBoyDocKind Kind)
	{
		switch (Kind)
		{
		case EProtoBoyDocKind::Game: return EProtoBoyDocLayer::Game;
		case EProtoBoyDocKind::Design: return EProtoBoyDocLayer::Design;
		case EProtoBoyDocKind::FeatureTech:
		case EProtoBoyDocKind::SharedTech: return EProtoBoyDocLayer::Tech;
		case EProtoBoyDocKind::Content:
		case EProtoBoyDocKind::StyleGuide: return EProtoBoyDocLayer::Content;
		}
		return EProtoBoyDocLayer::Design;
	}

	FText GetKindName(EProtoBoyDocKind Kind)
	{
		switch (Kind)
		{
		case EProtoBoyDocKind::Game: return LOCTEXT("KindGame", "Game");
		case EProtoBoyDocKind::Design: return LOCTEXT("KindDesign", "Design");
		case EProtoBoyDocKind::FeatureTech: return LOCTEXT("KindFeatureTech", "Tech");
		case EProtoBoyDocKind::SharedTech: return LOCTEXT("KindSharedTech", "Shared Tech");
		case EProtoBoyDocKind::Content: return LOCTEXT("KindContent", "Content");
		case EProtoBoyDocKind::StyleGuide: return LOCTEXT("KindStyleGuide", "Style Guide");
		}
		return FText::GetEmpty();
	}

	FText GetKindDescription(EProtoBoyDocKind Kind)
	{
		switch (Kind)
		{
		case EProtoBoyDocKind::Design: return GetDescription(EProtoBoyDocLayer::Design);
		case EProtoBoyDocKind::FeatureTech: return LOCTEXT("KindFeatureTechTip", "How a feature is built: one section for each section of its Design doc, answered from the engine side.");
		case EProtoBoyDocKind::SharedTech: return LOCTEXT("KindSharedTechTip", "Tech with no player face of its own, e.g. save/load, input, networking, the AI framework: responsibilities, interface, data, who uses it.");
		case EProtoBoyDocKind::Content: return LOCTEXT("KindContentTip", "What exists in the game, e.g. levels, items, quests, characters, balance tables: an inventory, authoring rules, where the data lives.");
		case EProtoBoyDocKind::StyleGuide: return LOCTEXT("KindStyleGuideTip", "How the game looks, sounds or reads, e.g. art, audio, UX, writing style: goals, style, references, budgets.");
		case EProtoBoyDocKind::Game: break;
		}
		return GetDescription(EProtoBoyDocLayer::Game);
	}

	TArray<EProtoBoyDocKind> GetAddableKinds()
	{
		return { EProtoBoyDocKind::Design, EProtoBoyDocKind::SharedTech, EProtoBoyDocKind::Content, EProtoBoyDocKind::StyleGuide };
	}

	bool FromTemplateKey(const FString& Key, EProtoBoyDocKind& OutKind)
	{
		const FString Trimmed = Key.TrimStartAndEnd().ToLower();
		if (Trimmed == TEXT("design")) { OutKind = EProtoBoyDocKind::Design; return true; }
		if (Trimmed == TEXT("tech")) { OutKind = EProtoBoyDocKind::SharedTech; return true; }
		if (Trimmed == TEXT("content")) { OutKind = EProtoBoyDocKind::Content; return true; }
		if (Trimmed == TEXT("style")) { OutKind = EProtoBoyDocKind::StyleGuide; return true; }
		return false;
	}

	FString GetFolder(EProtoBoyDocKind Kind)
	{
		switch (Kind)
		{
		case EProtoBoyDocKind::Design:
		case EProtoBoyDocKind::FeatureTech: return TEXT("Design");
		case EProtoBoyDocKind::SharedTech: return TEXT("Tech");
		case EProtoBoyDocKind::Content:
		case EProtoBoyDocKind::StyleGuide: return TEXT("Content");
		case EProtoBoyDocKind::Game: break;
		}
		return FString();
	}

	const TCHAR* GetDecisionsHeadingTitle()
	{
		return TEXT("Decisions");
	}

	FHeading GetDecisionsHeading()
	{
		return { GetDecisionsHeadingTitle(), TEXT("One dated entry per decision made here: what was decided, why, and what it rules out. Newest last.") };
	}

	TArray<FHeading> GetHeadings(EProtoBoyDocKind Kind)
	{
		TArray<FHeading> Headings;
		switch (Kind)
		{
		case EProtoBoyDocKind::Game:
			Headings = {
				{ TEXT("Pitch"), TEXT("One sentence: what the player does, and why it's fun.") },
				{ TEXT("Player Fantasy"), TEXT("Who the player gets to be, and how it should feel.") },
				{ TEXT("Design Pillars"), TEXT("3-5 experience goals. Every feature should serve at least one. One ### heading per pillar, with the feeling it gives and what it rules out. Replace the example.") },
				{ TEXT("Non-goals"), TEXT("What this game deliberately isn't, so nobody builds it by accident.") },
				{ TEXT("Core Loop"), TEXT("What the player does over and over, step by step.") },
				{ TEXT("Game Flow"), TEXT("How a session goes from start to finish: menus, progression, ending.") },
				{ TEXT("Areas"), TEXT("Every doc below this one. ProtoBoy keeps the list up to date; write anything else around it.") },
				{ TEXT("Glossary"), TEXT("Terms the team uses, so docs and agents mean the same thing by them.") },
			};
			break;
		case EProtoBoyDocKind::Design:
		case EProtoBoyDocKind::FeatureTech:
			Headings = {
				{ TEXT("Purpose"), TEXT("Which design pillars (in the Game doc) this serves, and how.") },
				{ TEXT("Player Experience"), TEXT("As a player, I can ... to feel ...") },
				{ TEXT("Rules"), TEXT("How it works, from the player's side. No implementation details.") },
				{ TEXT("What the Player Sees"), TEXT("Feedback, UI and presentation.") },
				{ TEXT("Edge Cases"), TEXT("What happens when things go wrong or meet each other.") },
				{ TEXT("Open Questions"), TEXT("What isn't decided yet.") },
			};
			if (Kind == EProtoBoyDocKind::FeatureTech)
			{
				for (FHeading& Heading : Headings)
				{
					Heading.Hint = GetTechSectionHint();
				}
			}
			break;
		case EProtoBoyDocKind::SharedTech:
			Headings = {
				{ TEXT("Responsibilities"), TEXT("What this owns, and what it doesn't.") },
				{ TEXT("Interface"), TEXT("How other code uses it: classes, functions, events.") },
				{ TEXT("Data"), TEXT("What it stores, and where. Tunable values and the assets that hold them.") },
				{ TEXT("Used By"), TEXT("Which features and other tech depend on it.") },
			};
			break;
		case EProtoBoyDocKind::Content:
			Headings = {
				{ TEXT("Inventory"), TEXT("What exists: a table of every item, level, character, ...") },
				{ TEXT("Authoring Rules"), TEXT("How to make a new one, and what makes a good one.") },
				{ TEXT("Where the Data Lives"), TEXT("Data assets, DataTables and folders.") },
				{ TEXT("Fed Into"), TEXT("Which features use this content.") },
			};
			break;
		case EProtoBoyDocKind::StyleGuide:
			Headings = {
				{ TEXT("Goals"), TEXT("What it should make the player feel.") },
				{ TEXT("Style"), TEXT("The rules that keep it consistent.") },
				{ TEXT("References"), TEXT("Images, tracks or examples, and what to take from each.") },
				{ TEXT("Budgets"), TEXT("Limits: polycounts, texture sizes, memory, word counts, ...") },
			};
			break;
		}
		Headings.Add(GetDecisionsHeading());
		return Headings;
	}

	FString GetTechSectionHint()
	{
		return TEXT("How the engine side answers the Design doc's section of the same name: classes and modules, data and tunables, interfaces to other docs, constraints.");
	}

	FString GetIntro(EProtoBoyDocKind Kind)
	{
		switch (Kind)
		{
		case EProtoBoyDocKind::Game: return TEXT("Why this game exists, what it is, and where everything is documented. Every other doc serves this one. Humans own it: agents read it before anything else, and only propose changes. ProtoBoy keeps the Areas list current.");
		case EProtoBoyDocKind::Design: return TEXT("What this feature is, from the player's side. How it's built goes in its Tech doc.");
		case EProtoBoyDocKind::SharedTech: return TEXT("Shared tech with no player face of its own.");
		case EProtoBoyDocKind::Content: return TEXT("What exists in the game, and how it's authored.");
		case EProtoBoyDocKind::StyleGuide: return TEXT("How the game looks, sounds or reads.");
		case EProtoBoyDocKind::FeatureTech: break;
		}
		return FString();
	}

	FString MakeBody(const FString& Title, const FString& Intro, const TArray<FHeading>& Headings, const FString& Lead)
	{
		FString Out = FString::Printf(TEXT("# %s\n"), *Title);
		if (!Intro.IsEmpty())
		{
			Out += FString::Printf(TEXT("\n> %s\n"), *Intro);
		}
		if (!Lead.TrimStartAndEnd().IsEmpty())
		{
			Out += FString::Printf(TEXT("\n%s\n"), *Lead.TrimStartAndEnd());
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
