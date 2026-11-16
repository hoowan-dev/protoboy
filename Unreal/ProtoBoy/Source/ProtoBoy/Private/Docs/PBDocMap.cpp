#include "Docs/PBDocMap.h"

#include "Docs/Obsidian/PBObsidianConnector.h"
#include "Docs/PBDocsService.h"
#include "Docs/PBDocTemplates.h"
#include "Docs/PBMarkdown.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "Docs/PBDocMapUtils.h"
#include "PBModule.h"
#include "Tasks/PBTaskProvider.h"

#define LOCTEXT_NAMESPACE "ProtoBoyDocMap"

FProtoBoyDocMap::FProtoBoyDocMap(FProtoBoyDocsService& InDocs)
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
		Node.bTaskCategory = FCString::ToBool(*Frontmatter.Get(ProtoBoyDocLayers::TaskCategoryKey).TrimStartAndEnd());
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

bool FProtoBoyDocMap::WriteMissingGameDoc(const TArray<FProtoBoyDocTemplatePillar>* Pillars, const FString& TemplateName, bool& bOutWrote, FText& OutError,
	const TArray<FProtoBoyDocTemplateSection>* Examples, bool bGenerated)
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
		Headings[ProtoBoyDocMap::PillarsHeadingIndex].Hint = bGenerated
			? FString(TEXT("3-5 experience goals. Every feature should serve at least one. An agent drafted these from the project: check them, one ### heading per pillar."))
			: FString::Printf(TEXT("3-5 experience goals. Every feature should serve at least one. These are suggestions from the %s template: rewrite them for your game, one ### heading per pillar."), *TemplateName);
		PillarsText.Reset();
		for (const FProtoBoyDocTemplatePillar& Pillar : *Pillars)
		{
			PillarsText += FString::Printf(TEXT("### %s\n\n- **Feeling:** %s.\n- **Rules out:** %s.\n\n"), *Pillar.Name, *Pillar.Feeling, *Pillar.RulesOut);
		}
		PillarsText.TrimEndInline();
		PillarsText += TEXT("\n");
	}
	const FString Hint = FString::Printf(TEXT("<!-- %s -->\n"), *Headings[ProtoBoyDocMap::PillarsHeadingIndex].Hint);
	const bool bExamples = Examples && !Examples->IsEmpty();
	FString Body = ProtoBoyDocLayers::MakeBody(GetProjectTitle(), ProtoBoyDocLayers::GetIntro(EProtoBoyDocKind::Game), Headings,
		bExamples ? ProtoBoyDocMap::MakeExampleNote(TemplateName, bGenerated) : FString(), ProtoBoyDocMap::ToSectionText(Examples));
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

bool FProtoBoyDocMap::CreateDoc(EProtoBoyDocKind Kind, const FString& InTitle, const FString& ParentId, FString& OutId, FText& OutError)
{
	const FString Title = InTitle.TrimStartAndEnd();
	if (Title.IsEmpty())
	{
		OutError = LOCTEXT("NoTitle", "Give the doc a title.");
		return false;
	}
	if (!IsAllowedTitle(Title, OutError))
	{
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

TArray<const FProtoBoyDocNode*> FProtoBoyDocMap::GetTaskCategories() const
{
	TArray<const FProtoBoyDocNode*> Categories;
	const FProtoBoyDocNode* Root = GetGame();
	if (!Root)
	{
		return Categories;
	}
	TArray<const FProtoBoyDocNode*> Stack = { Root };
	while (!Stack.IsEmpty())
	{
		const FProtoBoyDocNode* Doc = Stack.Pop();
		if (Doc->bTaskCategory)
		{
			Categories.Add(Doc);
		}
		const TArray<const FProtoBoyDocNode*> Children = GetChildren(Doc->Id);
		for (int32 Index = Children.Num() - 1; Index >= 0; --Index)
		{
			Stack.Push(Children[Index]);
		}
	}
	return Categories;
}

bool FProtoBoyDocMap::IsAllowedTitle(const FString& Title, FText& OutWhyNot)
{
	if (ProtoBoyTasks::IsReservedCategoryName(Title))
	{
		OutWhyNot = FText::Format(LOCTEXT("ReservedTitle", "A doc can't be called \"{0}\": the task board uses that name for tasks that belong to no doc. Pick another title."),
			FText::FromString(ProtoBoyTasks::UncategorizedName));
		return false;
	}
	return true;
}

bool FProtoBoyDocMap::SetTaskCategory(const FString& Id, bool bTaskCategory, FText& OutError)
{
	const FProtoBoyDocNode* Node = Find(Id);
	if (!Node)
	{
		OutError = LOCTEXT("NoDoc", "That doc is no longer on the map. Refresh and try again.");
		return false;
	}
	if (Node->bTaskCategory == bTaskCategory)
	{
		return true;
	}
	return WriteFrontmatter(Node->Path, { { ProtoBoyDocLayers::TaskCategoryKey, bTaskCategory ? TEXT("true") : FString() } }, OutError);
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
	// The title the map would show for it (see Load).
	if (!IsAllowedTitle(Doc->Title.IsEmpty() ? FPaths::GetBaseFilename(Doc->Path) : Doc->Title, OutError))
	{
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
