#include "Docs/PBDocMap.h"

#include "Docs/Obsidian/PBObsidianConnector.h"
#include "Docs/PBDocGenerator.h"
#include "Docs/PBDocTemplates.h"
#include "Docs/PBMarkdown.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "PBModule.h"

#define LOCTEXT_NAMESPACE "ProtoBoyDocMap"

namespace ProtoBoyDocMap
{
	/** "Design/Combat Basics.md" -> "design-combat-basics". */
	FString Slugify(const FString& Text)
	{
		FString Slug;
		bool bDash = false;
		for (const TCHAR C : Text)
		{
			if (FChar::IsAlnum(C))
			{
				Slug.AppendChar(FChar::ToLower(C));
				bDash = false;
			}
			else if (!bDash && !Slug.IsEmpty())
			{
				Slug.AppendChar(TEXT('-'));
				bDash = true;
			}
		}
		while (Slug.EndsWith(TEXT("-")))
		{
			Slug.LeftChopInline(1);
		}
		return Slug.IsEmpty() ? FString(TEXT("doc")) : Slug.Left(60);
	}

	/** A title made safe to use as a file name (Obsidian also dislikes # ^ [ ] in note names). */
	FString ToFileName(const FString& Title)
	{
		FString Out;
		for (const TCHAR C : Title.TrimStartAndEnd())
		{
			if (!FString(TEXT("<>:\"/\\|?*#^[]")).Contains(FString::Chr(C)) && C >= 32)
			{
				Out.AppendChar(C);
			}
		}
		Out.TrimStartAndEndInline();
		while (Out.EndsWith(TEXT(".")))
		{
			Out.LeftChopInline(1);
		}
		return Out.IsEmpty() ? FString(TEXT("Untitled")) : Out.Left(80);
	}

	/** Text with ProtoBoy's managed blocks (generated lists) taken out, markers included. */
	FString RemoveManagedBlocks(const FString& Text)
	{
		FString Out = Text;
		int32 Begin;
		while ((Begin = Out.Find(TEXT("<!-- protoboy:begin "), ESearchCase::CaseSensitive)) != INDEX_NONE)
		{
			const int32 EndMarker = Out.Find(TEXT("<!-- protoboy:end "), ESearchCase::CaseSensitive, ESearchDir::FromStart, Begin);
			const int32 EndClose = EndMarker == INDEX_NONE ? INDEX_NONE : Out.Find(TEXT("-->"), ESearchCase::CaseSensitive, ESearchDir::FromStart, EndMarker);
			if (EndClose == INDEX_NONE)
			{
				break;
			}
			Out = Out.Left(Begin) + Out.Mid(EndClose + 3);
		}
		return Out;
	}

	/** What a reader actually wrote in a section: no hints, no generated blocks. */
	FString Clean(const FString& Text)
	{
		return ProtoBoyMarkdown::StripComments(RemoveManagedBlocks(Text));
	}

	/** First line below the title that someone wrote (not a heading, quote, hint or generated list). */
	FString FindSummary(const FString& Text, int32 BodyStart)
	{
		TArray<FString> Lines;
		Clean(Text.Mid(BodyStart)).ParseIntoArrayLines(Lines);
		for (const FString& Raw : Lines)
		{
			const FString Line = Raw.TrimStartAndEnd();
			if (Line.IsEmpty() || Line.StartsWith(TEXT("#")) || Line.StartsWith(TEXT(">")) || Line.StartsWith(TEXT("---")))
			{
				continue;
			}
			return Line.Len() > 160 ? Line.Left(157) + TEXT("...") : Line;
		}
		return FString();
	}

	/** Sort key for children: layers in their top-to-bottom order. */
	int32 LayerOrder(EProtoBoyDocLayer Layer)
	{
		return ProtoBoyDocLayers::GetAllLayers().IndexOfByKey(Layer);
	}

	/** ADRs from the eight-layer version: kept as files, off the map (decisions now live in each doc). */
	bool IsRetiredDoc(const FString& Path, const ProtoBoyMarkdown::FFrontmatter& Frontmatter)
	{
		return ProtoBoyDocLayers::IsRetiredKey(Frontmatter.Get(ProtoBoyDocLayers::LayerKey))
			|| (Frontmatter.Get(ProtoBoyDocLayers::LayerKey).IsEmpty() && Path.StartsWith(TEXT("ADR/ADR-"), ESearchCase::IgnoreCase));
	}

	/**
	 * The folder a doc's children go in. A doc that already has its own folder (Design/Combat/Combat.md, or its Tech doc
	 * Combat.tech.md next to it) shares it; a flat doc (Content/Ammo.md) gets a folder named after it (Content/Ammo/).
	 */
	FString GetChildFolder(const FString& ParentPath)
	{
		const FString Dir = FPaths::GetPath(ParentPath);
		FString Base = FPaths::GetBaseFilename(ParentPath);
		Base.RemoveFromEnd(TEXT(".tech"), ESearchCase::IgnoreCase);
		return FPaths::GetCleanFilename(Dir).Equals(Base, ESearchCase::IgnoreCase) ? Dir : Dir / Base;
	}

	/**
	 * Where a new doc goes. Under a parent (ParentPath, null for the Game doc) it goes in the parent's child folder;
	 * otherwise in its kind's folder (Design/, Tech/, Content/). Design docs get a folder of their own on top, like the
	 * features they describe (Design/Combat/Weapons/Weapons.md), so their children can nest in it. The Game doc is
	 * at the docs root.
	 */
	FString MakeNewDocPath(EProtoBoyDocKind Kind, const FString& Title, const FString* ParentPath)
	{
		if (Kind == EProtoBoyDocKind::Game)
		{
			return FProtoBoyDocMap::GetGameDocPath();
		}
		const FString Name = ToFileName(Title);
		FString Folder = ParentPath ? GetChildFolder(*ParentPath) : ProtoBoyDocLayers::GetFolder(Kind);
		if (Kind == EProtoBoyDocKind::Design)
		{
			Folder /= Name;
		}
		return Folder / Name + TEXT(".md");
	}

	/** Slugify(Title), suffixed until it isn't in Used. Adds the result to Used. */
	FString TakeUniqueId(const FString& Title, TSet<FString>& Used)
	{
		const FString Base = Slugify(Title);
		FString Id = Base;
		for (int32 Suffix = 2; Used.Contains(Id); ++Suffix)
		{
			Id = FString::Printf(TEXT("%s-%d"), *Base, Suffix);
		}
		Used.Add(Id);
		return Id;
	}

	const TCHAR* ExamplePillar = TEXT(
		"### Example: Every Shot Counts\n"
		"\n"
		"- **Feeling:** tension; each decision matters.\n"
		"- **Rules out:** ammo pickups everywhere, spray-and-pray weapons.\n");

	/** "Design Pillars" is the third Game heading. */
	constexpr int32 PillarsHeadingIndex = 2;
}

FProtoBoyDocMap::FProtoBoyDocMap(FProtoBoyDocGenerator& InDocs)
	: Docs(InDocs)
{
}

// --- Loading ------------------------------------------------------------------------------------------------------

void FProtoBoyDocMap::Refresh()
{
	Load();
	OnChanged.Broadcast();
}

void FProtoBoyDocMap::EnsureLoaded() const
{
	if (!bLoaded)
	{
		Load();
	}
}

void FProtoBoyDocMap::Load() const
{
	bLoaded = true;
	Nodes.Reset();
	Unmapped.Reset();
	Parents.Reset();

	const IProtoBoyDocsProvider& Provider = Docs.GetProvider();
	FText WhyNot;
	if (!Provider.IsReady(WhyNot))
	{
		return;
	}

	TArray<FProtoBoyDocInfo> List = Provider.ListDocs();
	List.Sort([](const FProtoBoyDocInfo& A, const FProtoBoyDocInfo& B) { return A.Path < B.Path; });

	TSet<FString> UsedIds;
	for (const FProtoBoyDocInfo& Doc : List)
	{
		if (FPaths::GetCleanFilename(Doc.Path).Equals(FProtoBoyObsidianConnector::MarkerNoteName, ESearchCase::IgnoreCase))
		{
			continue;
		}
		FString Text;
		FText Ignored;
		if (!Provider.ReadDoc(Doc.Path, Text, Ignored))
		{
			continue;
		}

		const ProtoBoyMarkdown::FFrontmatter Frontmatter = ProtoBoyMarkdown::ParseFrontmatter(Text);
		if (ProtoBoyDocMap::IsRetiredDoc(Doc.Path, Frontmatter))
		{
			continue;
		}
		FProtoBoyDocNode Node;
		Node.LayerKey = Frontmatter.Get(ProtoBoyDocLayers::LayerKey).TrimStartAndEnd().ToLower();
		if (!ProtoBoyDocLayers::FromKey(Node.LayerKey, Node.Layer))
		{
			Unmapped.Add(Doc);
			continue;
		}

		// Ids must be unique; a copied doc keeps its original's id until someone changes it, so suffix duplicates.
		const FString BaseId = Frontmatter.Get(ProtoBoyDocLayers::IdKey).IsEmpty()
			? ProtoBoyDocMap::Slugify(FPaths::ChangeExtension(Doc.Path, TEXT("")))
			: Frontmatter.Get(ProtoBoyDocLayers::IdKey);
		Node.Id = BaseId;
		for (int32 Suffix = 2; UsedIds.Contains(Node.Id); ++Suffix)
		{
			Node.Id = FString::Printf(TEXT("%s-%d"), *BaseId, Suffix);
		}
		UsedIds.Add(Node.Id);

		Node.Path = Doc.Path;
		Node.Title = Doc.Title.IsEmpty() ? FPaths::GetBaseFilename(Doc.Path) : Doc.Title;
		Node.ParentId = Frontmatter.Get(ProtoBoyDocLayers::ParentKey);
		if (Node.ParentId.IsEmpty())
		{
			// An older feature Tech doc: it sat under the Design doc it implemented, and still does.
			Node.ParentId = Frontmatter.Get(ProtoBoyDocLayers::LegacyImplementsKey);
		}
		Node.Summary = ProtoBoyDocMap::FindSummary(Text, Frontmatter.BodyStart);
		Nodes.Add(MoveTemp(Node));
	}

	ResolveParents();
}

void FProtoBoyDocMap::ResolveParents() const
{
	const FProtoBoyDocNode* Root = GetGame();
	if (!Root)
	{
		return;
	}

	// First the frontmatter parent if it names another doc, else the layer default.
	for (const FProtoBoyDocNode& Node : Nodes)
	{
		if (&Node == Root)
		{
			Parents.Add(Node.Id, FString());
			continue;
		}
		const FProtoBoyDocNode* Explicit = Node.ParentId.IsEmpty() ? nullptr : Find(Node.ParentId);
		const FProtoBoyDocNode* Parent = Explicit && Explicit != &Node ? Explicit : GetDefaultParent(Node);
		Parents.Add(Node.Id, Parent && Parent != &Node ? Parent->Id : Root->Id);
	}

	// Then break loops (A under B under A): the doc where the walk comes back around goes straight under the root,
	// which ends every chain.
	for (const FProtoBoyDocNode& Start : Nodes)
	{
		TSet<FString> Seen;
		FString Current = Start.Id;
		while (!Current.IsEmpty())
		{
			if (Seen.Contains(Current))
			{
				Parents[Current] = Root->Id;
				Seen.Reset();
				Current = Start.Id;
				continue;
			}
			Seen.Add(Current);
			Current = Parents.FindRef(Current);
		}
	}
}

// --- Queries ------------------------------------------------------------------------------------------------------

const TArray<FProtoBoyDocNode>& FProtoBoyDocMap::GetNodes() const
{
	EnsureLoaded();
	return Nodes;
}

const TArray<FProtoBoyDocInfo>& FProtoBoyDocMap::GetUnmappedDocs() const
{
	EnsureLoaded();
	return Unmapped;
}

const FProtoBoyDocNode* FProtoBoyDocMap::Find(const FString& Id) const
{
	EnsureLoaded();
	return Id.IsEmpty() ? nullptr : Nodes.FindByPredicate([&Id](const FProtoBoyDocNode& Node) { return Node.Id == Id; });
}

const FProtoBoyDocNode* FProtoBoyDocMap::FindByPath(const FString& Path) const
{
	EnsureLoaded();
	return Nodes.FindByPredicate([&Path](const FProtoBoyDocNode& Node) { return Node.Path.Equals(Path, ESearchCase::IgnoreCase); });
}

const FProtoBoyDocNode* FProtoBoyDocMap::GetGame() const
{
	EnsureLoaded();
	// A real Game doc first; then an old Vision over an old Overview, since the Vision is the doc the pillars are in.
	for (const TCHAR* Key : { TEXT("game"), TEXT("vision") })
	{
		if (const FProtoBoyDocNode* Found = Nodes.FindByPredicate([Key](const FProtoBoyDocNode& Node)
		{
			return Node.Layer == EProtoBoyDocLayer::Game && Node.LayerKey == Key;
		}))
		{
			return Found;
		}
	}
	return Nodes.FindByPredicate([](const FProtoBoyDocNode& Node) { return Node.Layer == EProtoBoyDocLayer::Game; });
}

const FProtoBoyDocNode* FProtoBoyDocMap::GetDefaultParent(const FProtoBoyDocNode& Node) const
{
	const FProtoBoyDocNode* Root = GetGame();
	return &Node == Root ? nullptr : Root;
}

const FProtoBoyDocNode* FProtoBoyDocMap::GetParent(const FString& Id) const
{
	EnsureLoaded();
	const FString* ParentId = Parents.Find(Id);
	return ParentId ? Find(*ParentId) : nullptr;
}

TArray<const FProtoBoyDocNode*> FProtoBoyDocMap::GetChildren(const FString& Id) const
{
	EnsureLoaded();
	TArray<const FProtoBoyDocNode*> Children;
	for (const FProtoBoyDocNode& Node : Nodes)
	{
		if (const FString* ParentId = Parents.Find(Node.Id); ParentId && !ParentId->IsEmpty() && *ParentId == Id)
		{
			Children.Add(&Node);
		}
	}
	Children.StableSort([](const FProtoBoyDocNode& A, const FProtoBoyDocNode& B)
	{
		const int32 OrderA = ProtoBoyDocMap::LayerOrder(A.Layer);
		const int32 OrderB = ProtoBoyDocMap::LayerOrder(B.Layer);
		return OrderA != OrderB ? OrderA < OrderB : A.Title < B.Title;
	});
	return Children;
}

bool FProtoBoyDocMap::IsInSubtree(const FString& Id, const FString& Ancestor) const
{
	EnsureLoaded();
	FString Current = Id;
	for (int32 Guard = 0; !Current.IsEmpty() && Guard <= Nodes.Num(); ++Guard)
	{
		if (Current == Ancestor)
		{
			return true;
		}
		Current = Parents.FindRef(Current);
	}
	return false;
}

FString FProtoBoyDocMap::MakeUniqueId(const FString& Title) const
{
	const FString Base = ProtoBoyDocMap::Slugify(Title);
	FString Id = Base;
	for (int32 Suffix = 2; Find(Id); ++Suffix)
	{
		Id = FString::Printf(TEXT("%s-%d"), *Base, Suffix);
	}
	return Id;
}

FString FProtoBoyDocMap::MakeLink(const FProtoBoyDocNode& Node)
{
	return FString::Printf(TEXT("[[%s|%s]]"), *FPaths::ChangeExtension(Node.Path, TEXT("")), *Node.Title);
}

// --- Reading ------------------------------------------------------------------------------------------------------

FProtoBoyGameDoc FProtoBoyDocMap::ReadGame() const
{
	FProtoBoyGameDoc Game;
	const FProtoBoyDocNode* Node = GetGame();
	FString Text;
	FText Ignored;
	if (!Node || !Docs.GetProvider().ReadDoc(Node->Path, Text, Ignored))
	{
		return Game;
	}

	Game.bExists = true;
	Game.Path = Node->Path;
	Game.Pitch = ProtoBoyDocMap::Clean(ProtoBoyMarkdown::GetSectionBody(Text, TEXT("Pitch")));
	Game.PlayerFantasy = ProtoBoyDocMap::Clean(ProtoBoyMarkdown::GetSectionBody(Text, TEXT("Player Fantasy")));
	Game.NonGoals = ProtoBoyDocMap::Clean(ProtoBoyMarkdown::GetSectionBody(Text, TEXT("Non-goals")));
	Game.CoreLoop = ProtoBoyDocMap::Clean(ProtoBoyMarkdown::GetSectionBody(Text, TEXT("Core Loop")));

	const TArray<ProtoBoyMarkdown::FSection> Sections = ProtoBoyMarkdown::ParseSections(Text);
	if (const ProtoBoyMarkdown::FSection* PillarsSection = ProtoBoyMarkdown::FindSection(Sections, TEXT("Design Pillars")))
	{
		for (const ProtoBoyMarkdown::FSection& Section : Sections)
		{
			const bool bInside = Section.HeadingStart > PillarsSection->HeadingStart && Section.HeadingStart < PillarsSection->End;
			// The template's example isn't a real pillar.
			if (bInside && Section.Level == PillarsSection->Level + 1 && !Section.Heading.StartsWith(TEXT("Example"), ESearchCase::IgnoreCase))
			{
				FProtoBoyDesignPillar Pillar;
				Pillar.Name = Section.Heading;
				Pillar.Text = ProtoBoyDocMap::Clean(Text.Mid(Section.BodyStart, Section.End - Section.BodyStart));
				Game.Pillars.Add(MoveTemp(Pillar));
			}
		}
	}
	return Game;
}

// --- Writing ------------------------------------------------------------------------------------------------------

void FProtoBoyDocMap::AfterWrite()
{
	Load();
	FText Error;
	if (!UpdateAreasIndex(Error))
	{
		UE_LOG(LogProtoBoy, Warning, TEXT("Couldn't update the Game doc's Areas list: %s"), *Error.ToString());
	}
	OnChanged.Broadcast();
}

bool FProtoBoyDocMap::WriteFrontmatter(const FString& Path, const TArray<TPair<FString, FString>>& Values, FText& OutError)
{
	IProtoBoyDocsProvider& Provider = Docs.GetProvider();
	FString Text;
	if (!Provider.ReadDoc(Path, Text, OutError))
	{
		return false;
	}
	if (!Provider.WriteDoc(Path, ProtoBoyMarkdown::SetFrontmatterValues(Text, Values), /*bMustNotExist*/ false, OutError))
	{
		return false;
	}
	AfterWrite();
	return true;
}

bool FProtoBoyDocMap::WriteMissingGameDoc(const TArray<FProtoBoyDocTemplatePillar>* Pillars, const FString& TemplateName, bool& bOutWrote, FText& OutError)
{
	IProtoBoyDocsProvider& Provider = Docs.GetProvider();
	if (!Provider.IsReady(OutError))
	{
		return false;
	}
	if (GetGame())
	{
		return true;
	}
	const FString Path = GetGameDocPath();
	if (Provider.DocExists(Path))
	{
		OutError = FText::Format(LOCTEXT("ExistsNoLayer", "{0} already exists but isn't on the map. Link it as the Game doc from the doc map, or move or rename it, then try again."), FText::FromString(Path));
		return false;
	}

	// The Design Pillars section gets the template's pillars (placeholders to rewrite), or one example.
	TArray<ProtoBoyDocLayers::FHeading> Headings = ProtoBoyDocLayers::GetHeadings(EProtoBoyDocKind::Game);
	FString PillarsText = ProtoBoyDocMap::ExamplePillar;
	if (Pillars && !Pillars->IsEmpty())
	{
		Headings[ProtoBoyDocMap::PillarsHeadingIndex].Hint = FString::Printf(TEXT("3-5 experience goals. Every feature should serve at least one. These are suggestions from the %s template: rewrite them for your game, one ### heading per pillar."), *TemplateName);
		PillarsText.Reset();
		for (const FProtoBoyDocTemplatePillar& Pillar : *Pillars)
		{
			PillarsText += FString::Printf(TEXT("### %s\n\n- **Feeling:** %s.\n- **Rules out:** %s.\n\n"), *Pillar.Name, *Pillar.Feeling, *Pillar.RulesOut);
		}
		PillarsText.TrimEndInline();
		PillarsText += TEXT("\n");
	}
	const FString Hint = FString::Printf(TEXT("<!-- %s -->\n"), *Headings[ProtoBoyDocMap::PillarsHeadingIndex].Hint);
	FString Body = ProtoBoyDocLayers::MakeBody(GetProjectTitle(), ProtoBoyDocLayers::GetIntro(EProtoBoyDocKind::Game), Headings);
	Body.ReplaceInline(*Hint, *(Hint + TEXT("\n") + PillarsText));

	const FString Text = ProtoBoyMarkdown::SetFrontmatterValues(Body, {
		{ ProtoBoyDocLayers::IdKey, MakeUniqueId(TEXT("game")) },
		{ ProtoBoyDocLayers::LayerKey, ProtoBoyDocLayers::ToKey(EProtoBoyDocLayer::Game) },
	});
	if (!Provider.WriteDoc(Path, Text, /*bMustNotExist*/ true, OutError))
	{
		return false;
	}
	bOutWrote = true;
	return true;
}

TArray<const FProtoBoyDocNode*> FProtoBoyDocMap::GetGameDocs() const
{
	TArray<const FProtoBoyDocNode*> Games;
	const FProtoBoyDocNode* Root = GetGame();
	if (!Root)
	{
		return Games;
	}
	Games.Add(Root);
	for (const FProtoBoyDocNode& Node : Nodes)
	{
		if (Node.Layer == EProtoBoyDocLayer::Game && &Node != Root)
		{
			Games.Add(&Node);
		}
	}
	return Games;
}

FString FProtoBoyDocMap::GetProjectTitle()
{
	const FString Name = FApp::GetProjectName();
	return Name.IsEmpty() ? FString(TEXT("Game")) : Name;
}

FString FProtoBoyDocMap::GetGameDocPath()
{
	return ProtoBoyDocMap::ToFileName(GetProjectTitle()) + TEXT(".md");
}

bool FProtoBoyDocMap::CombineGameDocs(FText& OutError)
{
	const TArray<const FProtoBoyDocNode*> Games = GetGameDocs();
	if (Games.Num() < 2)
	{
		return true;
	}
	IProtoBoyDocsProvider& Provider = Docs.GetProvider();

	// Read every Game doc's "## " sections, the root's first.
	struct FDocSections
	{
		FString Path;
		FString Id;
		FString Frontmatter;
		TArray<TPair<FString, FString>> Sections;
	};
	TArray<FDocSections> Read;
	for (const FProtoBoyDocNode* Game : Games)
	{
		FString Text;
		if (!Provider.ReadDoc(Game->Path, Text, OutError))
		{
			return false;
		}
		FDocSections& Entry = Read.AddDefaulted_GetRef();
		Entry.Path = Game->Path;
		Entry.Id = Game->Id;
		Entry.Frontmatter = Text.Left(ProtoBoyMarkdown::ParseFrontmatter(Text).BodyStart);
		for (const ProtoBoyMarkdown::FSection& Section : ProtoBoyMarkdown::ParseSections(Text))
		{
			if (Section.Level == 2)
			{
				Entry.Sections.Emplace(Section.Heading, Text.Mid(Section.BodyStart, Section.End - Section.BodyStart).TrimStartAndEnd());
			}
		}
	}

	// The Game template's sections in order: the first written version wins (hints only if nobody wrote anything).
	TSet<FString> Used;
	FString Body = FString::Printf(TEXT("# %s\n\n> %s\n"), *GetProjectTitle(), *ProtoBoyDocLayers::GetIntro(EProtoBoyDocKind::Game));
	auto Matches = [](const FString& A, const FString& B) { return A.Equals(B, ESearchCase::IgnoreCase); };
	for (const ProtoBoyDocLayers::FHeading& Heading : ProtoBoyDocLayers::GetHeadings(EProtoBoyDocKind::Game))
	{
		Used.Add(Heading.Title.ToLower());
		const bool bDecisions = Matches(Heading.Title, ProtoBoyDocLayers::GetDecisionsHeadingTitle());
		TArray<FString> Written;
		FString Fallback;
		for (const FDocSections& Doc : Read)
		{
			for (const TPair<FString, FString>& Section : Doc.Sections)
			{
				if (!Matches(Section.Key, Heading.Title))
				{
					continue;
				}
				// The Areas index is regenerated; its old copies are dropped.
				const bool bHasWriting = !ProtoBoyDocMap::Clean(Section.Value).IsEmpty() && !Matches(Heading.Title, TEXT("Areas"));
				if (bHasWriting)
				{
					Written.Add(Section.Value);
				}
				else if (Fallback.IsEmpty())
				{
					Fallback = Section.Value;
				}
			}
		}
		FString Content;
		if (Written.IsEmpty())
		{
			Content = Heading.Hint.IsEmpty() ? FString() : FString::Printf(TEXT("<!-- %s -->"), *Heading.Hint);
		}
		else
		{
			Content = bDecisions ? FString::Join(Written, TEXT("\n")) : Written[0];
		}
		Body += FString::Printf(TEXT("\n## %s\n\n%s\n"), *Heading.Title, *Content);
	}
	// Everything else someone wrote (e.g. the old Vision's References, the old Overview's Summary), in order.
	for (const FDocSections& Doc : Read)
	{
		for (const TPair<FString, FString>& Section : Doc.Sections)
		{
			if (!Used.Contains(Section.Key.ToLower()) && !ProtoBoyDocMap::Clean(Section.Value).IsEmpty())
			{
				Used.Add(Section.Key.ToLower());
				Body += FString::Printf(TEXT("\n## %s\n\n%s\n"), *Section.Key, *Section.Value);
			}
		}
	}

	// <Project>.md keeps the root's other properties. It may be one of the docs being merged; the rest go to the trash.
	const FString Path = GetGameDocPath();
	const bool bPathIsMerged = Read.ContainsByPredicate([&Path](const FDocSections& Doc) { return Doc.Path.Equals(Path, ESearchCase::IgnoreCase); });
	if (!bPathIsMerged && Provider.DocExists(Path))
	{
		OutError = FText::Format(LOCTEXT("CombineTaken", "There's already a {0} that isn't on the map. Move or rename it, then combine again."), FText::FromString(Path));
		return false;
	}
	TSet<FString> OldIds;
	for (const FDocSections& Doc : Read)
	{
		OldIds.Add(Doc.Id);
	}
	TSet<FString> UsedIds;
	for (const FProtoBoyDocNode& Node : Nodes)
	{
		if (!OldIds.Contains(Node.Id))
		{
			UsedIds.Add(Node.Id);
		}
	}
	const FString NewId = ProtoBoyDocMap::TakeUniqueId(TEXT("game"), UsedIds);
	const FString Text = ProtoBoyMarkdown::SetFrontmatterValues(Read[0].Frontmatter + Body, {
		{ ProtoBoyDocLayers::IdKey, NewId },
		{ ProtoBoyDocLayers::LayerKey, ProtoBoyDocLayers::ToKey(EProtoBoyDocLayer::Game) },
		{ ProtoBoyDocLayers::ParentKey, FString() },
	});
	if (!Provider.WriteDoc(Path, Text, /*bMustNotExist*/ false, OutError))
	{
		return false;
	}

	// Docs that named an old Game doc as their parent now sit under the new one by default.
	TArray<FString> Children;
	for (const FProtoBoyDocNode& Node : Nodes)
	{
		if (!OldIds.Contains(Node.Id) && OldIds.Contains(Node.ParentId))
		{
			Children.Add(Node.Path);
		}
	}
	bool bOk = true;
	for (const FString& ChildPath : Children)
	{
		FString ChildText;
		bOk &= Provider.ReadDoc(ChildPath, ChildText, OutError)
			&& Provider.WriteDoc(ChildPath, ProtoBoyMarkdown::SetFrontmatterValues(ChildText, { { ProtoBoyDocLayers::ParentKey, FString() } }), false, OutError);
	}
	for (const FDocSections& Doc : Read)
	{
		if (!Doc.Path.Equals(Path, ESearchCase::IgnoreCase))
		{
			bOk &= Provider.DeleteDoc(Doc.Path, OutError);
		}
	}
	AfterWrite();
	return bOk;
}

bool FProtoBoyDocMap::DeleteDoc(const FString& Id, FText& OutError)
{
	const FProtoBoyDocNode* Node = Find(Id);
	if (!Node)
	{
		OutError = LOCTEXT("DeleteNoDoc", "That doc is no longer on the map. Refresh and try again.");
		return false;
	}
	if (Node == GetGame())
	{
		OutError = LOCTEXT("DeleteGame", "The Game doc is the top of the map; it can't be deleted from here.");
		return false;
	}

	IProtoBoyDocsProvider& Provider = Docs.GetProvider();
	const FString Path = Node->Path;
	TArray<FString> Children;
	for (const FProtoBoyDocNode& Other : Nodes)
	{
		if (Other.ParentId == Id)
		{
			Children.Add(Other.Path);
		}
	}
	if (!Provider.DeleteDoc(Path, OutError))
	{
		return false;
	}

	// Their parent is gone, so they go back under the Game doc. Clear the stale keys rather than leave them dangling.
	bool bOk = true;
	for (const FString& ChildPath : Children)
	{
		FString ChildText;
		bOk &= Provider.ReadDoc(ChildPath, ChildText, OutError)
			&& Provider.WriteDoc(ChildPath, ProtoBoyMarkdown::SetFrontmatterValues(ChildText, {
				{ ProtoBoyDocLayers::ParentKey, FString() },
				{ ProtoBoyDocLayers::LegacyImplementsKey, FString() },
			}), false, OutError);
	}
	AfterWrite();
	return bOk;
}

bool FProtoBoyDocMap::CreateGameDoc(FText& OutError)
{
	bool bWrote = false;
	const bool bOk = WriteMissingGameDoc(nullptr, FString(), bWrote, OutError);
	if (bWrote)
	{
		AfterWrite();
	}
	return bOk;
}

FProtoBoyDocTemplatePlan FProtoBoyDocMap::PlanTemplate(const FProtoBoyDocTemplate& Template) const
{
	// Read the docs again, so notes added, moved or renamed in Obsidian since the last refresh are replaced too.
	Load();
	FProtoBoyDocTemplatePlan Plan;
	Plan.NumToCreate = 1;
	for (const FProtoBoyDocNode& Node : Nodes)
	{
		Plan.Removed.Add(Node.Path);
	}

	// Files that aren't on the map but sit where the template writes go too, so the template can be written whole.
	const IProtoBoyDocsProvider& Provider = Docs.GetProvider();
	auto RemoveIfTaken = [&Plan, &Provider](const FString& Path)
	{
		if (Provider.DocExists(Path) && !Plan.Removed.ContainsByPredicate([&Path](const FString& Other) { return Other.Equals(Path, ESearchCase::IgnoreCase); }))
		{
			Plan.Removed.Add(Path);
		}
	};
	RemoveIfTaken(GetGameDocPath());
	TMap<FString, FString> Paths;
	TMap<FString, int32> Depths;
	for (const FProtoBoyDocTemplateDoc& Doc : Template.Docs)
	{
		FProtoBoyDocTemplatePlan::FEntry Entry;
		ProtoBoyDocLayers::FromTemplateKey(Doc.Kind, Entry.Kind);
		Entry.Title = Doc.Title.TrimStartAndEnd();
		Entry.Path = ProtoBoyDocMap::MakeNewDocPath(Entry.Kind, Entry.Title, Paths.Find(Doc.Parent));
		Entry.Depth = Doc.Parent.IsEmpty() ? 0 : Depths.FindRef(Doc.Parent) + 1;
		RemoveIfTaken(Entry.Path);
		++Plan.NumToCreate;

		Paths.Add(Doc.Id, Entry.Path);
		Depths.Add(Doc.Id, Entry.Depth);
		Plan.Docs.Add(MoveTemp(Entry));
	}
	return Plan;
}

bool FProtoBoyDocMap::ApplyTemplate(const FProtoBoyDocTemplate& Template, int32& OutCreated, FText& OutError)
{
	OutCreated = 0;
	IProtoBoyDocsProvider& Provider = Docs.GetProvider();
	if (!Provider.IsReady(OutError))
	{
		return false;
	}

	// Clear first: everything on the map, and whatever sits where the template writes, goes to the trash.
	const FProtoBoyDocTemplatePlan Plan = PlanTemplate(Template);
	for (const FString& Path : Plan.Removed)
	{
		if (!Provider.DeleteDoc(Path, OutError))
		{
			AfterWrite();
			return false;
		}
	}
	Load();

	bool bWrote = false;
	if (!WriteMissingGameDoc(&Template.Pillars, Template.Name, bWrote, OutError))
	{
		AfterWrite();
		return false;
	}
	OutCreated = bWrote ? 1 : 0;
	Load();

	// Written in order, so every parent exists (on disk, and in MapIds) before its children. The map is read again
	// once at the end instead of after every doc.
	const FProtoBoyDocNode* Root = GetGame();
	const FString RootId = Root ? Root->Id : FString();

	TSet<FString> UsedIds;
	for (const FProtoBoyDocNode& Node : Nodes)
	{
		UsedIds.Add(Node.Id);
	}
	TMap<FString, FString> MapIds;  // template doc id -> its id on the map ("" = not on the map)
	TMap<FString, FString> Paths;
	bool bOk = true;
	for (const FProtoBoyDocTemplateDoc& Doc : Template.Docs)
	{
		EProtoBoyDocKind Kind = EProtoBoyDocKind::Design;
		ProtoBoyDocLayers::FromTemplateKey(Doc.Kind, Kind);
		const FString Title = Doc.Title.TrimStartAndEnd();
		const FString Path = ProtoBoyDocMap::MakeNewDocPath(Kind, Title, Paths.Find(Doc.Parent));
		Paths.Add(Doc.Id, Path);

		const FString Id = ProtoBoyDocMap::TakeUniqueId(Title, UsedIds);
		const FString ParentId = Doc.Parent.IsEmpty() ? FString() : MapIds.FindRef(Doc.Parent);
		const FString Text = ProtoBoyMarkdown::SetFrontmatterValues(
			ProtoBoyDocLayers::MakeBody(Title, ProtoBoyDocLayers::GetIntro(Kind), ProtoBoyDocLayers::GetHeadings(Kind), Doc.Summary), {
				{ ProtoBoyDocLayers::IdKey, Id },
				{ ProtoBoyDocLayers::LayerKey, ProtoBoyDocLayers::ToKey(ProtoBoyDocLayers::GetLayer(Kind)) },
				{ ProtoBoyDocLayers::ParentKey, ParentId != RootId ? ParentId : FString() },
			});
		if (!Provider.WriteDoc(Path, Text, /*bMustNotExist*/ true, OutError))
		{
			bOk = false;
			break;
		}
		MapIds.Add(Doc.Id, Id);
		++OutCreated;
		bWrote = true;
	}

	// Folders the old docs leave empty go too, so the old structure doesn't linger. Each delete already removes the
	// folders it empties; this also catches folders emptied earlier (or kept by Obsidian holding them open), but only
	// under the top-level folders the removed docs were in and the kind folders, so other empty folders stay.
	TSet<FString> Folders = { TEXT("Design"), TEXT("Tech"), TEXT("Content"), TEXT("Systems"), TEXT("Direction") };
	for (const FString& Removed : Plan.Removed)
	{
		FString Top;
		if (Removed.Split(TEXT("/"), &Top, nullptr))
		{
			Folders.Add(Top);
		}
	}
	for (const FString& Folder : Folders)
	{
		Provider.PruneEmptyFolders(Folder);
	}

	AfterWrite();
	return bOk;
}

bool FProtoBoyDocMap::CreateDoc(EProtoBoyDocKind Kind, const FString& InTitle, const FString& ParentId, FString& OutId, FText& OutError)
{
	const FString Title = InTitle.TrimStartAndEnd();
	if (Title.IsEmpty())
	{
		OutError = LOCTEXT("NoTitle", "Give the doc a title.");
		return false;
	}
	if (!ProtoBoyDocLayers::GetAddableKinds().Contains(Kind))
	{
		OutError = LOCTEXT("NotAddable", "The Game doc is created from setup.");
		return false;
	}
	const FProtoBoyDocNode* Root = GetGame();
	if (!Root)
	{
		OutError = LOCTEXT("NeedsGame", "Create the Game doc first: every other doc hangs below it.");
		return false;
	}
	const FProtoBoyDocNode* Parent = ParentId.IsEmpty() ? nullptr : Find(ParentId);
	if (!ParentId.IsEmpty() && !Parent)
	{
		OutError = LOCTEXT("NoParent", "That parent doc is no longer on the map. Refresh and try again.");
		return false;
	}

	const FString Path = ProtoBoyDocMap::MakeNewDocPath(Kind, Title,
		Parent && Parent->Layer != EProtoBoyDocLayer::Game ? &Parent->Path : nullptr);

	IProtoBoyDocsProvider& Provider = Docs.GetProvider();
	if (Provider.DocExists(Path))
	{
		OutError = FText::Format(LOCTEXT("PathTaken", "There's already a doc at {0}."), FText::FromString(Path));
		return false;
	}

	// The default parent (the Game doc) isn't written, so docs follow it if it's ever replaced.
	OutId = MakeUniqueId(Title);
	const FString Text = ProtoBoyMarkdown::SetFrontmatterValues(
		ProtoBoyDocLayers::MakeBody(Title, ProtoBoyDocLayers::GetIntro(Kind), ProtoBoyDocLayers::GetHeadings(Kind)), {
			{ ProtoBoyDocLayers::IdKey, OutId },
			{ ProtoBoyDocLayers::LayerKey, ProtoBoyDocLayers::ToKey(ProtoBoyDocLayers::GetLayer(Kind)) },
			{ ProtoBoyDocLayers::ParentKey, Parent && Parent != Root ? Parent->Id : FString() },
		});
	if (!Provider.WriteDoc(Path, Text, /*bMustNotExist*/ true, OutError))
	{
		return false;
	}
	AfterWrite();
	return true;
}

bool FProtoBoyDocMap::SetParent(const FString& Id, const FString& NewParentId, FText& OutError)
{
	const FProtoBoyDocNode* Node = Find(Id);
	if (!Node)
	{
		OutError = LOCTEXT("NoDoc", "That doc is no longer on the map. Refresh and try again.");
		return false;
	}
	if (Node == GetGame())
	{
		OutError = LOCTEXT("MoveGame", "The Game doc is the top of the map; it can't go under another doc.");
		return false;
	}
	const FProtoBoyDocNode* NewParent = NewParentId.IsEmpty() ? GetDefaultParent(*Node) : Find(NewParentId);
	if (!NewParent)
	{
		OutError = LOCTEXT("NoNewParent", "That doc is no longer on the map. Refresh and try again.");
		return false;
	}
	if (IsInSubtree(NewParent->Id, Id))
	{
		OutError = LOCTEXT("Loop", "A doc can't go under itself or a doc below it.");
		return false;
	}
	if (GetParent(Id) == NewParent)
	{
		return true;
	}

	// The default parent isn't written, so the doc follows it if the Game doc is ever replaced. An old
	// "protoboy-implements" goes too, or it would put the doc back under its Design doc.
	const FString Value = NewParent == GetDefaultParent(*Node) ? FString() : NewParent->Id;
	return WriteFrontmatter(Node->Path, {
		{ ProtoBoyDocLayers::ParentKey, Value },
		{ ProtoBoyDocLayers::LegacyImplementsKey, FString() },
	}, OutError);
}

bool FProtoBoyDocMap::AddToMap(const FString& Path, EProtoBoyDocLayer Layer, const FString& ParentId, FText& OutError)
{
	if (FindByPath(Path))
	{
		OutError = FText::Format(LOCTEXT("AlreadyMapped", "{0} is already on the map."), FText::FromString(Path));
		return false;
	}
	const FProtoBoyDocInfo* Doc = GetUnmappedDocs().FindByPredicate([&Path](const FProtoBoyDocInfo& Info) { return Info.Path.Equals(Path, ESearchCase::IgnoreCase); });
	if (!Doc)
	{
		OutError = FText::Format(LOCTEXT("NotFound", "{0} isn't in the docs folder any more. Refresh and try again."), FText::FromString(Path));
		return false;
	}
	if (Layer == EProtoBoyDocLayer::Game && GetGame())
	{
		OutError = LOCTEXT("OnlyOneGame", "The map already has a Game doc.");
		return false;
	}

	const FProtoBoyDocNode* Parent = ParentId.IsEmpty() ? nullptr : Find(ParentId);
	if (!ParentId.IsEmpty() && !Parent)
	{
		OutError = LOCTEXT("MapNoParent", "That parent doc is no longer on the map. Refresh and try again.");
		return false;
	}

	const FString Title = Doc->Title.IsEmpty() ? FPaths::GetBaseFilename(Doc->Path) : Doc->Title;
	return WriteFrontmatter(Doc->Path, {
		{ ProtoBoyDocLayers::IdKey, MakeUniqueId(Title) },
		{ ProtoBoyDocLayers::LayerKey, ProtoBoyDocLayers::ToKey(Layer) },
		{ ProtoBoyDocLayers::LegacyImplementsKey, FString() },
		{ ProtoBoyDocLayers::ParentKey, Parent && Parent != GetGame() ? Parent->Id : FString() },
	}, OutError);
}

bool FProtoBoyDocMap::RecordDecision(const FString& Id, const FString& Decision, const FString& Why, const FString& RulesOut, const FString& By, FText& OutError)
{
	const FProtoBoyDocNode* Node = Find(Id);
	if (!Node)
	{
		OutError = LOCTEXT("DecisionNoDoc", "That doc isn't on the map.");
		return false;
	}
	if (Decision.TrimStartAndEnd().IsEmpty())
	{
		OutError = LOCTEXT("DecisionEmpty", "Say what was decided.");
		return false;
	}

	FString Entry = FString::Printf(TEXT("- **%s: %s**"), *FDateTime::Now().ToString(TEXT("%Y-%m-%d")), *Decision.TrimStartAndEnd());
	if (!Why.TrimStartAndEnd().IsEmpty())
	{
		Entry += FString::Printf(TEXT(" Why: %s"), *Why.TrimStartAndEnd());
	}
	if (!RulesOut.TrimStartAndEnd().IsEmpty())
	{
		Entry += FString::Printf(TEXT(" Rules out: %s"), *RulesOut.TrimStartAndEnd());
	}
	if (!By.IsEmpty())
	{
		Entry += FString::Printf(TEXT(" _(%s)_"), *By);
	}
	if (!Docs.AppendToSection(Node->Path, ProtoBoyDocLayers::GetDecisionsHeadingTitle(), Entry, OutError))
	{
		return false;
	}
	AfterWrite();
	return true;
}

bool FProtoBoyDocMap::UpdateAreasIndex(FText& OutError)
{
	const FProtoBoyDocNode* Root = GetGame();
	if (!Root)
	{
		return true;
	}

	// Grouped by layer at the top level, then nested the way the map nests.
	FString Content;
	TFunction<void(const FProtoBoyDocNode&, int32)> AddTree = [&](const FProtoBoyDocNode& Node, int32 Depth)
	{
		Content += FString::ChrN(Depth, TEXT('\t')) + FString::Printf(TEXT("- %s"), *MakeLink(Node));
		if (Depth > 0)
		{
			Content += FString::Printf(TEXT(" (%s)"), *ProtoBoyDocLayers::GetDisplayName(Node.Layer).ToString());
		}
		Content += TEXT("\n");
		for (const FProtoBoyDocNode* Child : GetChildren(Node.Id))
		{
			AddTree(*Child, Depth + 1);
		}
	};
	const TArray<const FProtoBoyDocNode*> TopLevel = GetChildren(Root->Id);
	for (const EProtoBoyDocLayer Layer : ProtoBoyDocLayers::GetAllLayers())
	{
		bool bHeader = false;
		for (const FProtoBoyDocNode* Node : TopLevel)
		{
			if (Node->Layer != Layer)
			{
				continue;
			}
			if (!bHeader)
			{
				Content += FString::Printf(TEXT("%s**%s**\n\n"), Content.IsEmpty() ? TEXT("") : TEXT("\n"), *ProtoBoyDocLayers::GetDisplayName(Layer).ToString());
				bHeader = true;
			}
			AddTree(*Node, 0);
		}
	}
	if (Content.IsEmpty())
	{
		Content = TEXT("_Nothing below the Game doc yet. Add docs from ProtoBoy's doc map._\n");
	}

	IProtoBoyDocsProvider& Provider = Docs.GetProvider();
	FString Text;
	if (!Provider.ReadDoc(Root->Path, Text, OutError))
	{
		return false;
	}
	const FString Updated = ProtoBoyMarkdown::SetManagedBlock(Text, TEXT("Areas"), TEXT("areas"), Content);
	return Updated == Text || Provider.WriteDoc(Root->Path, Updated, /*bMustNotExist*/ false, OutError);
}

#undef LOCTEXT_NAMESPACE
