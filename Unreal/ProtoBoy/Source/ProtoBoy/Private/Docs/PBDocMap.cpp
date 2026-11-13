#include "Docs/PBDocMap.h"

#include "Docs/Obsidian/PBObsidianConnector.h"
#include "Docs/PBDocGenerator.h"
#include "Docs/PBDocTemplates.h"
#include "Docs/PBMarkdown.h"
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
	 * Where a new doc goes: Design docs nest in folders like the features they describe (under ParentDesignPath's folder
	 * when the parent is a Design doc); the other kinds keep one flat folder each.
	 */
	FString MakeNewDocPath(EProtoBoyDocKind Kind, const FString& Title, const FString* ParentDesignPath)
	{
		const FString Name = ToFileName(Title);
		const FString Folder = Kind == EProtoBoyDocKind::Design
			? (ParentDesignPath ? FPaths::GetPath(*ParentDesignPath) : ProtoBoyDocLayers::GetFolder(Kind)) / Name
			: ProtoBoyDocLayers::GetFolder(Kind);
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
		Node.ImplementsId = Frontmatter.Get(ProtoBoyDocLayers::ImplementsKey);
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
	// An old Vision beats an old Overview: it's the doc the pillars are in.
	const FProtoBoyDocNode* Preferred = Nodes.FindByPredicate([](const FProtoBoyDocNode& Node)
	{
		return Node.Layer == EProtoBoyDocLayer::Game && Node.LayerKey != TEXT("overview");
	});
	return Preferred ? Preferred : Nodes.FindByPredicate([](const FProtoBoyDocNode& Node) { return Node.Layer == EProtoBoyDocLayer::Game; });
}

const FProtoBoyDocNode* FProtoBoyDocMap::GetDefaultParent(const FProtoBoyDocNode& Node) const
{
	const FProtoBoyDocNode* Root = GetGame();
	if (&Node == Root)
	{
		return nullptr;
	}
	if (Node.Layer == EProtoBoyDocLayer::Tech)
	{
		if (const FProtoBoyDocNode* Design = Find(Node.ImplementsId))
		{
			return Design;
		}
	}
	return Root;
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

const FProtoBoyDocNode* FProtoBoyDocMap::FindTechFor(const FString& DesignId) const
{
	EnsureLoaded();
	return Nodes.FindByPredicate([&DesignId](const FProtoBoyDocNode& Node)
	{
		return Node.Layer == EProtoBoyDocLayer::Tech && Node.ImplementsId == DesignId;
	});
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
	const FString Path = TEXT("Game.md");
	if (Provider.DocExists(Path))
	{
		OutError = FText::Format(LOCTEXT("ExistsNoLayer", "{0} already exists but isn't on the map. Add it with Map Existing Doc, as the Game doc."), FText::FromString(Path));
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
	FString Body = ProtoBoyDocLayers::MakeBody(TEXT("Game"), ProtoBoyDocLayers::GetIntro(EProtoBoyDocKind::Game), Headings);
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
	FProtoBoyDocTemplatePlan Plan;
	Plan.bCreatesGame = !GetGame();
	Plan.NumToCreate = Plan.bCreatesGame ? 1 : 0;

	const IProtoBoyDocsProvider& Provider = Docs.GetProvider();
	TMap<FString, FString> Paths;
	TMap<FString, int32> Depths;
	TSet<FString> DesignIds;
	for (const FProtoBoyDocTemplateDoc& Doc : Template.Docs)
	{
		FProtoBoyDocTemplatePlan::FEntry Entry;
		ProtoBoyDocLayers::FromTemplateKey(Doc.Kind, Entry.Kind);
		Entry.Title = Doc.Title.TrimStartAndEnd();
		const FString* ParentPath = DesignIds.Contains(Doc.Parent) ? Paths.Find(Doc.Parent) : nullptr;
		Entry.Path = ProtoBoyDocMap::MakeNewDocPath(Entry.Kind, Entry.Title, ParentPath);
		Entry.Depth = Doc.Parent.IsEmpty() ? 0 : Depths.FindRef(Doc.Parent) + 1;
		Entry.bExists = Provider.DocExists(Entry.Path);
		Plan.NumToCreate += Entry.bExists ? 0 : 1;

		Paths.Add(Doc.Id, Entry.Path);
		Depths.Add(Doc.Id, Entry.Depth);
		if (Entry.Kind == EProtoBoyDocKind::Design)
		{
			DesignIds.Add(Doc.Id);
		}
		Plan.Docs.Add(MoveTemp(Entry));
	}
	return Plan;
}

bool FProtoBoyDocMap::ApplyTemplate(const FProtoBoyDocTemplate& Template, int32& OutCreated, FText& OutError)
{
	OutCreated = 0;
	bool bWrote = false;
	if (!WriteMissingGameDoc(&Template.Pillars, Template.Name, bWrote, OutError))
	{
		return false;
	}
	OutCreated = bWrote ? 1 : 0;
	Load();

	// Written in order, so every parent exists (on disk, and in MapIds) before its children. The map is read again
	// once at the end instead of after every doc.
	const FProtoBoyDocNode* Root = GetGame();
	const FString RootId = Root ? Root->Id : FString();
	IProtoBoyDocsProvider& Provider = Docs.GetProvider();

	TSet<FString> UsedIds;
	for (const FProtoBoyDocNode& Node : Nodes)
	{
		UsedIds.Add(Node.Id);
	}
	TMap<FString, FString> MapIds;  // template doc id -> its id on the map ("" = not on the map)
	TMap<FString, FString> Paths;
	TSet<FString> DesignIds;
	bool bOk = true;
	for (const FProtoBoyDocTemplateDoc& Doc : Template.Docs)
	{
		EProtoBoyDocKind Kind = EProtoBoyDocKind::Design;
		ProtoBoyDocLayers::FromTemplateKey(Doc.Kind, Kind);
		const FString Title = Doc.Title.TrimStartAndEnd();
		const FString* ParentPath = DesignIds.Contains(Doc.Parent) ? Paths.Find(Doc.Parent) : nullptr;
		const FString Path = ProtoBoyDocMap::MakeNewDocPath(Kind, Title, ParentPath);
		Paths.Add(Doc.Id, Path);
		if (Kind == EProtoBoyDocKind::Design)
		{
			DesignIds.Add(Doc.Id);
		}

		// Never overwrite: keep what's there, and hang the template's children under it if it's on the map.
		if (Provider.DocExists(Path))
		{
			const FProtoBoyDocNode* Existing = FindByPath(Path);
			MapIds.Add(Doc.Id, Existing ? Existing->Id : FString());
			continue;
		}

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

	if (bWrote)
	{
		AfterWrite();
	}
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
		OutError = LOCTEXT("NotAddable", "The Game doc is created from setup, and a feature's Tech doc with Add Tech Doc.");
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
		Parent && Parent->Layer == EProtoBoyDocLayer::Design ? &Parent->Path : nullptr);

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

bool FProtoBoyDocMap::CreateTechDoc(const FString& DesignId, FString& OutId, FText& OutError)
{
	const FProtoBoyDocNode* Found = Find(DesignId);
	if (!Found || Found->Layer != EProtoBoyDocLayer::Design)
	{
		OutError = LOCTEXT("TechNeedsDesign", "Pick a Design doc to add its Tech doc.");
		return false;
	}
	if (const FProtoBoyDocNode* Existing = FindTechFor(DesignId))
	{
		OutError = FText::Format(LOCTEXT("TechExists", "{0} already has a Tech doc: {1}."), FText::FromString(Found->Title), FText::FromString(Existing->Path));
		return false;
	}
	const FProtoBoyDocNode Design = *Found;

	IProtoBoyDocsProvider& Provider = Docs.GetProvider();
	FString DesignText;
	if (!Provider.ReadDoc(Design.Path, DesignText, OutError))
	{
		return false;
	}

	// One section per "## " section of the Design doc, so each design question gets its engine-side answer. The
	// Design doc's Decisions stay there; the Tech doc gets its own.
	TArray<ProtoBoyDocLayers::FHeading> Headings;
	for (const ProtoBoyMarkdown::FSection& Section : ProtoBoyMarkdown::ParseSections(DesignText))
	{
		if (Section.Level == 2 && !Section.Heading.Equals(ProtoBoyDocLayers::GetDecisionsHeadingTitle(), ESearchCase::IgnoreCase))
		{
			Headings.Add({ Section.Heading, ProtoBoyDocLayers::GetTechSectionHint() });
		}
	}
	if (Headings.IsEmpty())
	{
		Headings = ProtoBoyDocLayers::GetHeadings(EProtoBoyDocKind::FeatureTech);
	}
	else
	{
		Headings.Add(ProtoBoyDocLayers::GetDecisionsHeading());
	}

	const FString Directory = FPaths::GetPath(Design.Path);
	const FString FileName = FPaths::GetBaseFilename(Design.Path) + TEXT(".tech.md");
	const FString Path = Directory.IsEmpty() ? FileName : Directory / FileName;
	if (Provider.DocExists(Path))
	{
		OutError = FText::Format(LOCTEXT("TechPathTaken", "There's already a doc at {0}. Add it with Map Existing Doc, or move it."), FText::FromString(Path));
		return false;
	}

	OutId = MakeUniqueId(Design.Id + TEXT("-tech"));
	const FString Intro = FString::Printf(TEXT("How %s is built. Each section answers the Design doc's section of the same name."), *MakeLink(Design));
	const FString Text = ProtoBoyMarkdown::SetFrontmatterValues(
		ProtoBoyDocLayers::MakeBody(FString::Printf(TEXT("%s (Tech)"), *Design.Title), Intro, Headings), {
			{ ProtoBoyDocLayers::IdKey, OutId },
			{ ProtoBoyDocLayers::LayerKey, ProtoBoyDocLayers::ToKey(EProtoBoyDocLayer::Tech) },
			{ ProtoBoyDocLayers::ImplementsKey, Design.Id },
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

	// The default parent isn't written, so the doc follows it if the default ever changes.
	const FString Value = NewParent == GetDefaultParent(*Node) ? FString() : NewParent->Id;
	return WriteFrontmatter(Node->Path, { { ProtoBoyDocLayers::ParentKey, Value } }, OutError);
}

bool FProtoBoyDocMap::AddToMap(const FString& Path, EProtoBoyDocLayer Layer, FText& OutError)
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

	// A mapped Tech doc is shared tech; link it to a Design doc with protoboy-implements to make it a feature's.
	const FString Title = Doc->Title.IsEmpty() ? FPaths::GetBaseFilename(Doc->Path) : Doc->Title;
	return WriteFrontmatter(Doc->Path, {
		{ ProtoBoyDocLayers::IdKey, MakeUniqueId(Title) },
		{ ProtoBoyDocLayers::LayerKey, ProtoBoyDocLayers::ToKey(Layer) },
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
