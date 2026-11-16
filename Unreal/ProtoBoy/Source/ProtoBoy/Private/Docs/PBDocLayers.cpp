#include "Docs/PBDocLayers.h"

#define LOCTEXT_NAMESPACE "ProtoBoyDocLayers"

namespace ProtoBoyDocLayers
{
	const TCHAR* IdKey = TEXT("protoboy-id");
	const TCHAR* LayerKey = TEXT("protoboy-layer");
	const TCHAR* ParentKey = TEXT("protoboy-parent");
	const TCHAR* TaskCategoryKey = TEXT("protoboy-task-category");
	const TCHAR* LegacyImplementsKey = TEXT("protoboy-implements");

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
		// Older versions' keys, so docs written then stay on the map without being rewritten.
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
		if (Trimmed == TEXT("direction") || Trimmed == TEXT("style"))
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
		case EProtoBoyDocLayer::Game: return LOCTEXT("GameTip", "The game as a whole: pitch, design pillars, non-goals, core loop, and an index of every other doc. One doc, named after the project, at the top of the map.");
		case EProtoBoyDocLayer::Design: return LOCTEXT("DesignTip", "What a player should be able to do or see.");
		case EProtoBoyDocLayer::Tech: return LOCTEXT("TechTip", "The technical details of how something works.");
		case EProtoBoyDocLayer::Content: return LOCTEXT("ContentTip", "The assets needed to make it happen, and the style they must have. Agents read it before looking for assets on storefronts.");
		}
		return FText::GetEmpty();
	}

	EProtoBoyDocLayer GetLayer(EProtoBoyDocKind Kind)
	{
		switch (Kind)
		{
		case EProtoBoyDocKind::Game: return EProtoBoyDocLayer::Game;
		case EProtoBoyDocKind::Design: return EProtoBoyDocLayer::Design;
		case EProtoBoyDocKind::Tech: return EProtoBoyDocLayer::Tech;
		case EProtoBoyDocKind::Content: return EProtoBoyDocLayer::Content;
		}
		return EProtoBoyDocLayer::Design;
	}

	EProtoBoyDocKind GetKind(EProtoBoyDocLayer Layer)
	{
		switch (Layer)
		{
		case EProtoBoyDocLayer::Game: return EProtoBoyDocKind::Game;
		case EProtoBoyDocLayer::Design: return EProtoBoyDocKind::Design;
		case EProtoBoyDocLayer::Tech: return EProtoBoyDocKind::Tech;
		case EProtoBoyDocLayer::Content: return EProtoBoyDocKind::Content;
		}
		return EProtoBoyDocKind::Design;
	}

	FText GetKindName(EProtoBoyDocKind Kind)
	{
		return GetDisplayName(GetLayer(Kind));
	}

	FText GetKindDescription(EProtoBoyDocKind Kind)
	{
		return GetDescription(GetLayer(Kind));
	}

	TArray<EProtoBoyDocKind> GetAddableKinds()
	{
		return { EProtoBoyDocKind::Design, EProtoBoyDocKind::Tech, EProtoBoyDocKind::Content };
	}

	bool FromTemplateKey(const FString& Key, EProtoBoyDocKind& OutKind)
	{
		const FString Trimmed = Key.TrimStartAndEnd().ToLower();
		if (Trimmed == TEXT("design")) { OutKind = EProtoBoyDocKind::Design; return true; }
		if (Trimmed == TEXT("tech")) { OutKind = EProtoBoyDocKind::Tech; return true; }
		if (Trimmed == TEXT("content")) { OutKind = EProtoBoyDocKind::Content; return true; }
		return false;
	}

	FString GetFolder(EProtoBoyDocKind Kind)
	{
		switch (Kind)
		{
		case EProtoBoyDocKind::Design: return TEXT("Design");
		case EProtoBoyDocKind::Tech: return TEXT("Tech");
		case EProtoBoyDocKind::Content: return TEXT("Content");
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
			Headings = {
				{ TEXT("Purpose"), TEXT("Which design pillars (in the Game doc) this serves, and how.") },
				{ TEXT("The Player Can"), TEXT("What the player should be able to do. As a player, I can ... to feel ...") },
				{ TEXT("The Player Sees"), TEXT("What the player should see and hear: feedback, UI and presentation.") },
				{ TEXT("Rules"), TEXT("How it works, from the player's side. How it's built goes in a Tech doc.") },
				{ TEXT("Edge Cases"), TEXT("What happens when things go wrong or meet each other.") },
				{ TEXT("Open Questions"), TEXT("What isn't decided yet.") },
			};
			break;
		case EProtoBoyDocKind::Tech:
			Headings = {
				{ TEXT("How It Works"), TEXT("The technical approach in a few paragraphs: the flow, from input to result.") },
				{ TEXT("Code"), TEXT("Modules, classes and functions, and what each one owns.") },
				{ TEXT("Data"), TEXT("What's stored and where: data assets, DataTables, config, save data. Tunable values.") },
				{ TEXT("Dependencies"), TEXT("What this uses, and what uses it.") },
				{ TEXT("Constraints"), TEXT("Performance budgets, networking, platform limits and other rules the code must follow.") },
			};
			break;
		case EProtoBoyDocKind::Content:
			Headings = {
				{ TEXT("Assets Needed"), TEXT("A table of every asset: name, type (mesh, texture, animation, sound, VFX, UI, data), status (placeholder, sourced, final) and source.") },
				{ TEXT("Style"), TEXT("How these assets should look and sound, e.g. stylized low-poly, hand-painted, pixel art, realistic PBR: palette, shapes, level of detail, and references (games, films, images) with what to take from each.") },
				{ TEXT("What to Look For"), TEXT("For anyone searching asset stores: search terms and tags that fit, the art styles to filter by, and technical needs (poly counts, texture sizes, rigged or not, Nanite, naming).") },
				{ TEXT("What to Avoid"), TEXT("Styles and assets that clash with the game, even if they're good.") },
				{ TEXT("Sourcing"), TEXT("Where assets come from: made by the team, an asset store (which listings), or placeholders until then.") },
				{ TEXT("Where They Live"), TEXT("Content folders, data assets and DataTables.") },
			};
			break;
		}
		Headings.Add(GetDecisionsHeading());
		return Headings;
	}

	FString GetIntro(EProtoBoyDocKind Kind)
	{
		switch (Kind)
		{
		case EProtoBoyDocKind::Game: return TEXT("What this game is, and where everything is documented. Every other doc serves this one. Humans own it: agents read it before anything else, and only propose changes. ProtoBoy keeps the Areas list current.");
		case EProtoBoyDocKind::Design: return TEXT("What a player should be able to do or see.");
		case EProtoBoyDocKind::Tech: return TEXT("The technical details of how this works.");
		case EProtoBoyDocKind::Content: return TEXT("The assets needed to make this happen, and the style they must have. Agents read this before looking for or choosing assets.");
		}
		return FString();
	}

	FString MakeBody(const FString& Title, const FString& Intro, const TArray<FHeading>& Headings, const FString& Lead,
		const TArray<TPair<FString, FString>>& SectionText)
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
			const TPair<FString, FString>* Text = SectionText.FindByPredicate([&Heading](const TPair<FString, FString>& Entry)
			{
				return Entry.Key.Equals(Heading.Title, ESearchCase::IgnoreCase);
			});
			if (Text && !Text->Value.TrimStartAndEnd().IsEmpty())
			{
				Out += FString::Printf(TEXT("\n%s\n"), *Text->Value.TrimStartAndEnd());
			}
		}
		return Out;
	}
}

#undef LOCTEXT_NAMESPACE
