#include "Docs/PBDocMap.h"

#include "Docs/Obsidian/PBObsidianConnector.h"
#include "Docs/PBDocGenerator.h"
#include "Docs/PBMarkdown.h"
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

	const TCHAR* VisionExamplePillar = TEXT(
		"### Example: Every Shot Counts\n"
		"\n"
		"- **Feeling:** tension; each decision matters.\n"
		"- **Rules out:** ammo pickups everywhere, spray-and-pray weapons.\n");
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
		FProtoBoyDocNode Node;
		if (!ProtoBoyDocLayers::FromKey(Frontmatter.Get(ProtoBoyDocLayers::LayerKey), Node.Layer))
		{
			// ADRs written before layers existed are still decisions.
			const FString AdrPrefix = ProtoBoyDocLayers::GetFolder(EProtoBoyDocLayer::Decision) + TEXT("/ADR-");
			if (!Doc.Path.StartsWith(AdrPrefix, ESearchCase::IgnoreCase))
			{
				Unmapped.Add(Doc);
				continue;
			}
			Node.Layer = EProtoBoyDocLayer::Decision;
			Node.bInferred = true;
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
	const FProtoBoyDocNode* Vision = GetVision();
	if (!Vision)
	{
		return;
	}

	// First the frontmatter parent if it names another doc, else the layer default.
	for (const FProtoBoyDocNode& Node : Nodes)
	{
		if (&Node == Vision)
		{
			Parents.Add(Node.Id, FString());
			continue;
		}
		const FProtoBoyDocNode* Explicit = Node.ParentId.IsEmpty() ? nullptr : Find(Node.ParentId);
		const FProtoBoyDocNode* Parent = Explicit && Explicit != &Node ? Explicit : GetDefaultParent(Node);
		Parents.Add(Node.Id, Parent && Parent != &Node ? Parent->Id : Vision->Id);
	}

	// Then break loops (A under B under A): the doc where the walk comes back around falls back to the Overview, or
	// the Vision for the Overview itself. Each pass fixes at least one loop, and the Vision ends every chain.
	const FProtoBoyDocNode* Overview = GetOverview();
	for (const FProtoBoyDocNode& Start : Nodes)
	{
		TSet<FString> Seen;
		FString Current = Start.Id;
		while (!Current.IsEmpty())
		{
			if (Seen.Contains(Current))
			{
				const bool bIsOverview = Overview && Current == Overview->Id;
				Parents[Current] = (!bIsOverview && Overview && !Seen.Contains(Overview->Id)) ? Overview->Id : Vision->Id;
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

const FProtoBoyDocNode* FProtoBoyDocMap::GetVision() const
{
	EnsureLoaded();
	return Nodes.FindByPredicate([](const FProtoBoyDocNode& Node) { return Node.Layer == EProtoBoyDocLayer::Vision; });
}

const FProtoBoyDocNode* FProtoBoyDocMap::GetOverview() const
{
	EnsureLoaded();
	return Nodes.FindByPredicate([](const FProtoBoyDocNode& Node) { return Node.Layer == EProtoBoyDocLayer::Overview; });
}

const FProtoBoyDocNode* FProtoBoyDocMap::GetDefaultParent(const FProtoBoyDocNode& Node) const
{
	const FProtoBoyDocNode* Vision = GetVision();
	const FProtoBoyDocNode* Overview = GetOverview();
	switch (Node.Layer)
	{
	case EProtoBoyDocLayer::Vision:
	case EProtoBoyDocLayer::Overview:
		// A second Vision or Overview sits under the real one's parent level, so it stays visible.
		return &Node == Vision ? nullptr : Vision;
	case EProtoBoyDocLayer::Tech:
		if (const FProtoBoyDocNode* Design = Find(Node.ImplementsId))
		{
			return Design;
		}
		break;
	default:
		break;
	}
	return Overview ? Overview : Vision;
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

FProtoBoyVision FProtoBoyDocMap::ReadVision() const
{
	FProtoBoyVision Vision;
	const FProtoBoyDocNode* Node = GetVision();
	FString Text;
	FText Ignored;
	if (!Node || !Docs.GetProvider().ReadDoc(Node->Path, Text, Ignored))
	{
		return Vision;
	}

	Vision.bExists = true;
	Vision.Path = Node->Path;
	Vision.Pitch = ProtoBoyDocMap::Clean(ProtoBoyMarkdown::GetSectionBody(Text, TEXT("Pitch")));
	Vision.PlayerFantasy = ProtoBoyDocMap::Clean(ProtoBoyMarkdown::GetSectionBody(Text, TEXT("Player Fantasy")));
	Vision.NonGoals = ProtoBoyDocMap::Clean(ProtoBoyMarkdown::GetSectionBody(Text, TEXT("Non-goals")));

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
				Vision.Pillars.Add(MoveTemp(Pillar));
			}
		}
	}
	return Vision;
}

FString FProtoBoyDocMap::ReadOverviewSummary() const
{
	const FProtoBoyDocNode* Node = GetOverview();
	FString Text;
	FText Ignored;
	if (!Node || !Docs.GetProvider().ReadDoc(Node->Path, Text, Ignored))
	{
		return FString();
	}
	return ProtoBoyDocMap::Clean(ProtoBoyMarkdown::GetSectionBody(Text, TEXT("Summary")));
}

// --- Writing ------------------------------------------------------------------------------------------------------

void FProtoBoyDocMap::AfterWrite()
{
	Load();
	FText Error;
	if (!UpdateOverviewIndex(Error))
	{
		UE_LOG(LogProtoBoy, Warning, TEXT("Couldn't update the Overview's Areas list: %s"), *Error.ToString());
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

bool FProtoBoyDocMap::CreateVisionAndOverview(FText& OutError)
{
	IProtoBoyDocsProvider& Provider = Docs.GetProvider();
	if (!Provider.IsReady(OutError))
	{
		return false;
	}

	auto Create = [this, &Provider, &OutError](EProtoBoyDocLayer Layer, const FString& Title, const FString& Path, const FString& Body)
	{
		if (Provider.DocExists(Path))
		{
			OutError = FText::Format(LOCTEXT("ExistsNoLayer", "{0} already exists but isn't on the map. Add it with Map Existing Doc, as the {1}."),
				FText::FromString(Path), ProtoBoyDocLayers::GetDisplayName(Layer));
			return false;
		}
		const FString Text = ProtoBoyMarkdown::SetFrontmatterValues(Body, {
			{ ProtoBoyDocLayers::IdKey, MakeUniqueId(Title) },
			{ ProtoBoyDocLayers::LayerKey, ProtoBoyDocLayers::ToKey(Layer) },
		});
		return Provider.WriteDoc(Path, Text, /*bMustNotExist*/ true, OutError);
	};

	bool bWrote = false;
	bool bOk = true;
	if (!GetVision())
	{
		const FString Hint = FString::Printf(TEXT("<!-- %s -->\n"), *ProtoBoyDocLayers::GetHeadings(EProtoBoyDocLayer::Vision)[2].Hint);
		FString Body = ProtoBoyDocLayers::MakeBody(TEXT("Vision"), ProtoBoyDocLayers::GetIntro(EProtoBoyDocLayer::Vision),
			ProtoBoyDocLayers::GetHeadings(EProtoBoyDocLayer::Vision));
		Body.ReplaceInline(*Hint, *(Hint + TEXT("\n") + ProtoBoyDocMap::VisionExamplePillar));
		bOk = Create(EProtoBoyDocLayer::Vision, TEXT("Vision"), TEXT("Vision.md"), Body);
		bWrote |= bOk;
	}
	if (bOk && !GetOverview())
	{
		bOk = Create(EProtoBoyDocLayer::Overview, TEXT("Overview"), TEXT("Overview.md"),
			ProtoBoyDocLayers::MakeBody(TEXT("Overview"), ProtoBoyDocLayers::GetIntro(EProtoBoyDocLayer::Overview),
				ProtoBoyDocLayers::GetHeadings(EProtoBoyDocLayer::Overview)));
		bWrote |= bOk;
	}
	if (bWrote)
	{
		AfterWrite();
	}
	return bOk;
}

bool FProtoBoyDocMap::CreateDoc(EProtoBoyDocLayer Layer, const FString& InTitle, const FString& ParentId, FString& OutId, FText& OutError)
{
	const FString Title = InTitle.TrimStartAndEnd();
	if (Title.IsEmpty())
	{
		OutError = LOCTEXT("NoTitle", "Give the doc a title.");
		return false;
	}
	if (!ProtoBoyDocLayers::GetAddableLayers().Contains(Layer))
	{
		OutError = LOCTEXT("NotAddable", "Vision and Overview docs are created from setup, Tech docs with Add Tech Doc, and decisions as ADRs.");
		return false;
	}
	if (!HasVisionAndOverview())
	{
		OutError = LOCTEXT("NeedsVision", "Create the Vision and Overview first: every other doc hangs below them.");
		return false;
	}
	const FProtoBoyDocNode* Parent = ParentId.IsEmpty() ? nullptr : Find(ParentId);
	if (!ParentId.IsEmpty() && !Parent)
	{
		OutError = LOCTEXT("NoParent", "That parent doc is no longer on the map. Refresh and try again.");
		return false;
	}

	// Design docs nest like the features they describe; the other layers keep one flat folder each.
	const FString Name = ProtoBoyDocMap::ToFileName(Title);
	const FString Folder = Layer == EProtoBoyDocLayer::Design
		? (Parent && Parent->Layer == EProtoBoyDocLayer::Design ? FPaths::GetPath(Parent->Path) : ProtoBoyDocLayers::GetFolder(Layer)) / Name
		: ProtoBoyDocLayers::GetFolder(Layer);
	const FString Path = Folder / Name + TEXT(".md");

	IProtoBoyDocsProvider& Provider = Docs.GetProvider();
	if (Provider.DocExists(Path))
	{
		OutError = FText::Format(LOCTEXT("PathTaken", "There's already a doc at {0}."), FText::FromString(Path));
		return false;
	}

	// The default parent (the Overview) isn't written, so moving the Overview never strands anything.
	const FProtoBoyDocNode* DefaultParent = GetOverview();
	OutId = MakeUniqueId(Title);
	const FString Text = ProtoBoyMarkdown::SetFrontmatterValues(
		ProtoBoyDocLayers::MakeBody(Title, ProtoBoyDocLayers::GetIntro(Layer), ProtoBoyDocLayers::GetHeadings(Layer)), {
			{ ProtoBoyDocLayers::IdKey, OutId },
			{ ProtoBoyDocLayers::LayerKey, ProtoBoyDocLayers::ToKey(Layer) },
			{ ProtoBoyDocLayers::ParentKey, Parent && Parent != DefaultParent ? Parent->Id : FString() },
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

	// One section per "## " section of the Design doc, so each design question gets its engine-side answer.
	TArray<ProtoBoyDocLayers::FHeading> Headings;
	for (const ProtoBoyMarkdown::FSection& Section : ProtoBoyMarkdown::ParseSections(DesignText))
	{
		if (Section.Level == 2)
		{
			Headings.Add({ Section.Heading, ProtoBoyDocLayers::GetTechSectionHint() });
		}
	}
	if (Headings.IsEmpty())
	{
		for (const ProtoBoyDocLayers::FHeading& Heading : ProtoBoyDocLayers::GetHeadings(EProtoBoyDocLayer::Design))
		{
			Headings.Add({ Heading.Title, ProtoBoyDocLayers::GetTechSectionHint() });
		}
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
	if (Node == GetVision())
	{
		OutError = LOCTEXT("MoveVision", "The Vision is the top of the map; it can't go under another doc.");
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
	if (Layer == EProtoBoyDocLayer::Tech)
	{
		OutError = LOCTEXT("MapTech", "Tech docs belong to a Design doc: select the Design doc and use Add Tech Doc.");
		return false;
	}
	if ((Layer == EProtoBoyDocLayer::Vision && GetVision()) || (Layer == EProtoBoyDocLayer::Overview && GetOverview()))
	{
		OutError = FText::Format(LOCTEXT("OnlyOne", "The map already has a {0} doc."), ProtoBoyDocLayers::GetDisplayName(Layer));
		return false;
	}

	const FString Title = Doc->Title.IsEmpty() ? FPaths::GetBaseFilename(Doc->Path) : Doc->Title;
	return WriteFrontmatter(Doc->Path, {
		{ ProtoBoyDocLayers::IdKey, MakeUniqueId(Title) },
		{ ProtoBoyDocLayers::LayerKey, ProtoBoyDocLayers::ToKey(Layer) },
	}, OutError);
}

bool FProtoBoyDocMap::UpdateOverviewIndex(FText& OutError)
{
	const FProtoBoyDocNode* Overview = GetOverview();
	if (!Overview)
	{
		return true;
	}

	// Grouped by layer at the top level, then nested the way the map nests.
	FString Content;
	TFunction<void(const FProtoBoyDocNode&, int32)> AddTree = [&](const FProtoBoyDocNode& Node, int32 Depth)
	{
		Content += FString::ChrN(Depth, TEXT('\t')) + FString::Printf(TEXT("- %s"), *MakeLink(Node));
		if (Depth > 0 || Node.Layer == EProtoBoyDocLayer::Tech)
		{
			Content += FString::Printf(TEXT(" (%s)"), *ProtoBoyDocLayers::GetDisplayName(Node.Layer).ToString());
		}
		Content += TEXT("\n");
		for (const FProtoBoyDocNode* Child : GetChildren(Node.Id))
		{
			AddTree(*Child, Depth + 1);
		}
	};
	const TArray<const FProtoBoyDocNode*> TopLevel = GetChildren(Overview->Id);
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
		Content = TEXT("_Nothing below the Overview yet. Add docs from ProtoBoy's doc map._\n");
	}

	IProtoBoyDocsProvider& Provider = Docs.GetProvider();
	FString Text;
	if (!Provider.ReadDoc(Overview->Path, Text, OutError))
	{
		return false;
	}
	const FString Updated = ProtoBoyMarkdown::SetManagedBlock(Text, TEXT("Areas"), TEXT("areas"), Content);
	return Updated == Text || Provider.WriteDoc(Overview->Path, Updated, /*bMustNotExist*/ false, OutError);
}

#undef LOCTEXT_NAMESPACE
