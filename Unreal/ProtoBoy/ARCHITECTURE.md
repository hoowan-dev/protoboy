# ProtoBoy plugin architecture

Details of each feature area of the Unreal plugin. [CLAUDE.md](../../CLAUDE.md) has the build rules, conventions and a
one-line map; read the section here for the area you're changing.

Paths are relative to `Source/ProtoBoy/` (`Public/` or `Private/`).

## Core

- Single module `ProtoBoy` (Type `Editor`, `PostEngineInit`).
- `FProtoBoyModule` handles only editor UI registration: the `ProtoBoyDashboard` nomad tab and the
  **Tools > ProtoBoy Dashboard** menu entry (via a `UToolMenus` startup callback, owner-scoped so shutdown unregisters it).
- `UProtoBoySubsystem` (a `UEditorSubsystem`) is the central hub. It owns one plain C++ service per area as
  `TUniquePtr` and is where editor events (asset registry, package save, live coding) should be bound and routed
  (`BindEditorEvents`, currently a TODO). Access it with `UProtoBoySubsystem::Get()`.
  - The service headers are included directly in `PBSubsystem.h`, not forward-declared: UHT-generated code destroys the
    `TUniquePtr` members, and forward declarations fail with C4150 (incomplete type).
- **Product goal:** keep AI agents' work aligned with the game's architecture, design docs and task board, so they don't
  build monolithic or goal-less code. Layers: sources (provider interfaces) -> model (the doc map: the Game doc, then
  Design, Tech and Content docs, each with its Decisions; the task board) -> MCP tools -> workflows (agent guide, server
  instructions, prompts).
- `UProtoBoySettings` (`UDeveloperSettings`, `Config=ProtoBoy`, `DefaultConfig`) holds all team tunables and saves to the
  host project's `DefaultProtoBoy.ini`, which is committed. Machine-specific and per-user state (absolute paths,
  connections, tokens) goes in JSON stores under `Saved/ProtoBoy/` instead.

## Networking (Net/)

- Outgoing HTTP goes through `ProtoBoyHttp::SendRequest` (Private/Net/PBHttpClient.h): requests complete on the HTTP
  thread and their callbacks run from a queue the subsystem pumps (core ticker + modal tick).
  `ProtoBoyHttp::RunAfter` runs a delayed callback from the same pump (for retries; it keeps working in modal loops).
  `FResponse::RetryAfterSeconds` carries a `Retry-After` header.
- **Modal windows:** Slate's modal loop doesn't tick the core ticker, so local HTTP listeners and game-thread HTTP
  completions would stall while one is open. `UProtoBoySubsystem::OnModalLoopTick` pumps them.
- Helpers for the local HTTP listeners are in Private/Net/PBHttpServerUtils.h.

## Docs (Docs/)

### Storage and the docs service

- `IProtoBoyDocsProvider` (Docs/PBDocsProvider.h): docs storage. `FProtoBoyFolderDocsProvider` (Markdown files under a
  root, path-checked) backs both `FProtoBoyObsidianDocsProvider` (Docs/Obsidian/) and `FProtoBoyLocalDocsProvider`
  (`DocsDirectory`). Get the active one with `UProtoBoySubsystem::GetDocsProvider()`.
  - `DeleteDoc` moves the file to `GetTrashDir()` (the vault's `.trash` for Obsidian, else `Saved/ProtoBoy/DeletedDocs`)
    and removes the folders it leaves empty. `PruneEmptyFolders` cleans up after templates.
  - Folder providers retry writes, moves and folder deletes briefly (`ProtoBoyDocsProvider::WithRetries`), since
    Obsidian holds just-changed files and folders open on Windows.
- `FProtoBoyDocsService` (Docs/PBDocsService.h): the plain read/write/search layer over the active provider, under the
  doc map. Search, `CreateDoc`, `AppendToSection`, `SetManagedBlock`.
- **Writes never rewrite human text:** append, new files, or `<!-- protoboy:begin id -->` managed blocks
  (`ProtoBoyMarkdown`, Docs/PBMarkdown.h). Section hints are HTML comments, hidden in Obsidian and stripped by
  `ProtoBoyMarkdown::StripComments` when reading.
- **`FString::Find` clamps its start position** to the last character; line loops use
  `ProtoBoyMarkdown::Detail::FindLineEnd` (see CLAUDE.md).

### Obsidian (Docs/Obsidian/)

- `FProtoBoyObsidianConnector`: file-system link to an Obsidian vault (a folder with `.obsidian/`). Finds vaults via
  `%APPDATA%/obsidian/obsidian.json`, validates, creates the docs folder and a `_ProtoBoy.md` marker note, and saves the
  connection through `FProtoBoyDocsConnectionStore`. `OnConnectionChanged` fires after Connect/Disconnect.
- `FProtoBoyDocsConnectionStore` (Docs/PBDocsConnection.h) serializes the user's documentation connection
  (`FProtoBoyDocsConnection` USTRUCT, via `FJsonObjectConverter`) to `Saved/ProtoBoy/DocsConnection.json`. Add fields
  to the struct; bump `CurrentVersion` only for breaking changes.
- UI: `SProtoBoyObsidianPanel` (UI/Docs/Obsidian/), inside `SProtoBoyConnectDocsDialog` (UI/Docs/).

### The doc map (Docs/PBDocMap.h)

`FProtoBoyDocMap` is implemented in `PBDocMap.cpp` (loading, the tree, editing), `PBDocMapTemplates.cpp` (templates)
and `PBDocMapMigration.cpp` (merging old Game docs), with shared helpers in Private/Docs/PBDocMapUtils.h.

- **Layers** (`EProtoBoyDocLayer` in Docs/PBDocLayers.h, one `EProtoBoyDocKind` template each):
  - **Game:** one doc, the root, `<Project>.md` via `GetGameDocPath`: pitch, design pillars, non-goals, core loop,
    game flow, a generated Areas index, glossary. `CreateGameDoc` titles it `GetProjectTitle()`; the root node always
    shows the project's name.
  - **Design:** what a player should be able to do or see.
  - **Tech:** the technical details of how something works.
  - **Content:** the assets to make it happen and the style they must have (Style / What to Look For / What to Avoid,
    read by agents before searching storefronts). There's no separate style doc or art direction setting.
  - Headings per kind are in `ProtoBoyDocLayers::GetHeadings`. Every kind ends with a **Decisions** section: decisions
    live in the doc they affect (`RecordDecision`, MCP `record_decision`), not in separate ADRs.
- **The docs are the model.** Each doc's layer, stable id and parent live in its own YAML frontmatter (`protoboy-id`,
  `protoboy-layer`, `protoboy-parent`; keys in `ProtoBoyDocLayers`, read and written by
  `ProtoBoyMarkdown::ParseFrontmatter` / `SetFrontmatterValues`, which keep the team's other properties). The map is
  rebuilt by reading every doc (`Refresh`, also on docs connection changes; lazy on first use). There is no separate
  model file, so never add one: write changes into the docs.
- **Tree:** the Game doc is always the root; any other doc can go under any doc (no layer rules), defaulting to the
  Game doc. A doc's parent is its `protoboy-parent` if that names another doc, else the Game doc (`GetDefaultParent`),
  resolved once per load with loops broken (`ResolveParents`). Use `GetParent` / `GetChildren`, never the raw
  `ParentId`. Default parents aren't written, so `SetParent` clears the key when a doc goes back under the Game doc.
  Moving a doc on the map only changes its frontmatter, never its file.
- **Task categories:** any doc can be marked one (`protoboy-task-category: true`, removed when unchecked;
  `FProtoBoyDocNode::bTaskCategory`, `SetTaskCategory`, written to the file immediately). `GetTaskCategories` lists them
  in map order. "Uncategorized" is reserved (`ProtoBoyTasks::UncategorizedName`): `IsAllowedTitle` refuses it in
  `CreateDoc`, `AddToMap` and `ApplyTemplate` (before anything is trashed), and the doc generation prompt tells agents.
- **Creating docs:** `CreateDoc` (addable kinds: Design, Tech, Content) puts the file where its parent is
  (`MakeNewDocPath`): under the Game doc in the kind's folder (`Design/`, `Tech/`, `Content/`), under any other doc in
  that doc's folder (`GetChildFolder`: a doc's own folder if it has one, e.g. `Design/Combat/` for `Combat.md`, else a
  folder named after it, `Content/Ammo/` for `Content/Ammo.md`). Design docs get a folder of their own
  (`Design/Combat/Weapons/Weapons.md`); the Game doc always goes at the docs root. Templates use the same rule.
- **Other edits:** `DeleteDoc` (never the Game doc) goes through the provider's `DeleteDoc`, then clears the
  `protoboy-parent` keys that pointed at it. Docs without a layer are listed by `GetUnmappedDocs` and put on the map with
  `AddToMap` (takes a parent; refuses a second Game doc). Every write refreshes the map and rewrites the Game doc's
  managed `areas` block (`UpdateAreasIndex`), then fires `OnChanged`. `ReadGame` parses pitch / player fantasy / `###`
  design pillars (skipping `Example`) / non-goals / core loop for `get_project_context`.
- **Older docs still parse:** `protoboy-implements` is read as the parent when there's no `protoboy-parent`
  (`LegacyImplementsKey`; cleared on any move). `FromKey` maps vision and overview to Game, systems to Tech, direction
  and style to Content. An existing `Game.md` stays the Game doc. `GetGame` prefers a `game` doc, then an old Vision,
  then an old Overview. `CombineGameDocs` merges several Game-layer docs into one `<Project>.md` (each Game section from
  the first doc that wrote it, other sections after, old files to the trash; the map shows a banner while
  `HasSplitGameDocs`). ADRs (`decision`, or in `ADR/`) are skipped entirely. There's no feature-Tech pairing any more
  (no `FeatureTech` / `SharedTech` / `StyleGuide` kinds). The old pillar tree is in git history (commit 1139b1f).

### Doc map editor (UI/Docs/Map/)

- `SProtoBoyDocMapEditor`: a Behavior Tree style `SGraphEditor` graph (`UProtoBoyDocMapGraph` /
  `UProtoBoyDocMapGraphNode` / `UProtoBoyDocMapGraphSchema`), a transient view rebuilt after every change, deferred a
  frame since changes arrive inside graph callbacks.
- Node widget `SProtoBoyDocMapNode` reuses the AIGraph module's `SGraphPinAI` and `FAIGraphConnectionDrawingPolicy`.
  `SProtoBoyDocMapNode::GetLayerColor` is the one layer palette. Task categories show a clipboard icon.
- Docs are added only by dragging off a node (or right-clicking empty space, for the root): `MakeAddMenu` has a title
  box and every addable kind (`GetKindsUnder`: the parent layer's usual kinds first, Enter takes the first), plus Link
  an Existing Note. There are no toolbar add buttons. Delete (key or context menu) asks with `FMessageDialog` first.
- The task category toggle is in the node's context menu and in the details pane under the graph.
- The dashboard's doc map lock waits on the agent, docs connection and task board, not on the Game doc existing, since
  the map is where it gets created.

### Doc templates and generation

- Doc-set templates are `FProtoBoyDocTemplate` USTRUCTs (Docs/PBDocTemplates.h): design pillars for the Game doc plus a
  tree of docs, each with a `Kind` (design, tech, content; `ProtoBoyDocLayers::FromTemplateKey`) and a `Parent` (another
  template doc's id, parents first; the built-in ones put Tech and Content docs under the Design doc they serve), plus
  optional example documentation (`FProtoBoyDocTemplateSection` heading/text pairs: `GameExamples`, and `Examples` per
  doc, written when the confirm step's **Add example documentation** is picked).
- One JSON file each in the plugin's `Resources/DocTemplates/`, read and validated by
  `ProtoBoyDocTemplates::LoadTemplates()`. They're generated by `Scripts/make_doc_templates.py` (examples in
  `Scripts/doc_template_examples.py`); `Scripts/` is git-ignored on purpose, so the JSON is what's committed. Edit
  templates in the script when you have it and rerun it, otherwise edit the JSON.
- `PlanTemplate` / `ApplyTemplate` replace the documentation: every doc on the map (the Game doc included) plus any file
  at a path the template writes goes to the trash (`FProtoBoyDocTemplatePlan::Removed`), then a new Game doc with the
  template's pillars and the template's docs are written (same paths as `CreateDoc`). Unmapped notes elsewhere are kept.
  Empty folders are pruned; one refresh at the end. `PlanTemplate` re-reads the docs first.
- UI: `SProtoBoySetupDocsDialog` (UI/Docs/Map/), opened from the doc map's toolbar and empty-map prompt and from the
  Obsidian page of Connect to Documentation. A start step chooses between a template (pick with a preview, then a
  confirm step listing what moves to the trash and what's written) and generating from the project.
- **Generate from the Project** (`EStep::FromProject`, then `EStep::Generating` with the progress log, Cancel and
  Finish; the close button goes through `OnWindowCloseRequested`, same as Cancel). The work is
  `FProtoBoyDocGenerationRun` (Docs/Generation/) on `FProtoBoyAgentRun`. The prompt includes an asset registry summary
  of /Game because agents can't read .uasset files. The JSON answer is an `FProtoBoyDocTemplate` with `bGenerated`,
  applied with `ApplyTemplate(..., bExamples = true, ...)`. `ClearDocs` empties the map at start and on cancel.

## Tasks (Tasks/)

### Provider and Trello

- `IProtoBoyTaskProvider` (Tasks/PBTaskProvider.h): async task board API (callbacks on the game thread). Get it with
  `UProtoBoySubsystem::GetTaskProvider()`. Tasks link a design doc with a `Design doc: <path>` line.
  - `FProtoBoyTaskCategory` is a category (its doc's id and title). `CreateTasks` puts tasks in their categories' To Do
    lists, by `FProtoBoyNewTask::CategoryId` (used by task generation and by `task_create`'s optional `category`).
- `FProtoBoyTrelloConnector` (Tasks/Trello/): one-click connect. Opens `trello.com/1/authorize` with the API key from
  `UProtoBoySettings::TrelloApiKey` (else a built-in key); Trello redirects to a short-lived page on
  `127.0.0.1:<TasksCallbackPort>` that POSTs the token (from the URL fragment) back with a one-time state value. The
  token is verified against `/1/members/me` and saved per user via `FProtoBoyTasksConnectionStore`
  (`Saved/ProtoBoy/TasksConnection.json`, which also holds `LastTaskSyncUtc`). The key's Power-Up must allow that origin.
- `FProtoBoyTrelloClient` implements the provider, limited to the team's board and list mapping in `UProtoBoySettings`
  (`TrelloBoardId`, `Trello*ListId`, set in the Trello panel). One class in three files: `PBTrelloClient.cpp` (REST and
  provider), `PBTrelloBoardSetup.cpp` (board picker and template), `PBTrelloCategorySync.cpp` (category lists and
  labels); the plan struct is in `PBTrelloCategorySync.h`, shared JSON helpers in Private/Tasks/Trello/PBTrelloJson.h.
  - Calls Trello rate-limits (HTTP 429, nothing done) are retried after `Retry-After` or a doubling delay, up to
    `MaxRetries` (`CallAttempt`, through `ProtoBoyHttp::RunAfter`).
- UI: `SProtoBoyConnectTasksDialog` (UI/Tasks/) with provider panels in UI/Tasks/<Provider>/.

### Task categories on the board

- The board needs one `To Do - Uncategorized` list, plus one `To Do - <Category>` list and one label named after each
  category (`GetCategoryListName` / `ParseCategoryListName`). Other lists are ignored, and so are the lists mapped to In
  Progress / Done.
- **Lists and labels are tied to the category doc's id**, remembered in the team settings
  (`TrelloCategoryListIds`, `TrelloCategoryLabelIds`; `RememberCategoryIds`). Renaming a doc renames its list and label
  (`ListsToRename`, `LabelsToRename`) and its cards stay. Lists and labels with no remembered id are matched by name and
  remembered from then on (after an in-sync check, and at the end of every resync).
- `PlanCategorySync` only reads: it builds `FProtoBoyTrelloCategorySync` (`BuildCategoryPlan`) with the lists to
  create, rename and remove, labels to create and rename, and card label changes, plus every kept list's cards and the
  In Progress and Done cards.
- `ApplyCategorySync` carries it out in order: renames, new labels (palette colors), new lists (after the existing To
  Do lists), each extra list's cards moved to Uncategorized (`moveAllCards`) and the list archived (Trello can't delete
  lists), then card labels. Every card in a category's list carries that category's label and no other category's;
  cards in or moved to Uncategorized lose category labels. Labels of former categories are left on the board. If the
  mapped `TrelloToDoListId` was archived it's remapped to Uncategorized.

### The task board service (Tasks/PBTaskBoard.h)

- `FProtoBoyTaskBoard`, owned by the subsystem (`GetTaskBoard()`), is the board as ProtoBoy last saw it: the sync state
  (`ESyncState`), the tasks in each category's list (`FindList(DocId)`, empty id = Uncategorized), In Progress and Done,
  and the last sync time. `OnChanged` fires when any of it changes.
- `CheckSync(OnOutOfSync)` plans and loads; `ApplySync(Plan)` resyncs and checks again. The state resets to unknown when
  the categories change. The Tasks page, the out-of-sync window and task generation all read it here; nothing is read
  from the board until a check.

### Tasks page and windows (UI/Tasks/)

- `SProtoBoyTasksPage`, a view of the task board: the dashboard passes in the setup lock (`IsUnlocked`, `LockMessage`).
  - Top row: **Sync with <service>**, the check or red X, "Last synced <date>", errors, and **Generate Tasks**.
  - **To Do:** one expandable row per category in doc map order (plus Uncategorized, last) with its To Do count;
    opening it lists the cards in its list. Rows are keyed by doc id, so a renamed category stays open.
  - **In Progress** (all cards, not only the user's) and **Done** (most recently active first, the first 20), with the
    cards' category labels as chips (`FillTaskSection`).
- `SProtoBoyTrelloSyncDialog` (UI/Tasks/Trello/, not modal): lists what a resync changes and runs
  `FProtoBoyTaskBoard::ApplySync`.
- `SProtoBoyGenerateTasksDialog` (modal), three steps:
  1. **Setup:** what it does, the `generate-tasks` MCP prompt as the in-agent alternative, the agent
     (`SProtoBoyAgentPicker`), task category checkboxes (at least one), optional extra context.
  2. **Generating:** the agent log; Cancel asks first (Close once done); Next.
  3. **Review:** the ideas as nested checkboxes (each picked category, then Uncategorized; a category's box is
     tri-state), all ticked; Done adds the ticked ones and checks the board again, Cancel discards them (asks first, as
     does the close button).
- The work is `FProtoBoyTaskGenerationRun` (Tasks/Generation/) on `FProtoBoyAgentRun`. The prompt carries each picked
  category's doc and the docs below it (read through the provider, since a vault may be outside the project folder), the
  titles already in its To Do list (from the task board) and the extra context. The agent answers
  `{"tasks": [{category, title, description}]}`, which become `GetIdeas()`; nothing goes to the board until
  `AddTasks(Indices)` adds the kept ones with `CreateTasks` (the description links the category doc). Cancel stops the
  agent or stops adding; added tasks stay.

## Agents (Agents/)

- `FProtoBoyAgentRun` (Agents/PBAgentRun.h) runs one of the user's agents unattended (`claude -p --output-format
  stream-json` or `gemini -p` in the project folder, read-only tools, prompt file on stdin), turns its output into a
  progress log and extracts its JSON answer. Polled from the window's active timer (the windows are modal). Shared by
  doc generation and task generation; each keeps its prompt and raw output in `Saved/ProtoBoy/<Kind>Generation/`.
  Asset search and the doc health checks use it too, polled from their pages.
- `SProtoBoyAgentPicker` (UI/Agents/) is the agent radio list both generation windows, Search For Assets and the Health
  page use.
- `ProtoBoyAgentPrompt` (Private/Agents/PBAgentPromptContext.h) is the prompt context several runs share:
  `DescribeProjectAssets()` (an asset registry summary of /Game; agents can't read .uasset files) and `ReadDoc()` (a doc's
  text without frontmatter and hints; docs may live outside the project folder, where the agent can't read).

## MCP (MCP/)

- `FProtoBoyMCPServer`: ProtoBoy as an MCP server on `127.0.0.1` that agents (Claude Code, Claude Desktop, Gemini CLI)
  connect to; ProtoBoy holds no LLM API keys. Streamable HTTP via the engine's `HTTPServer` module (handlers run on the
  game thread). Dual-era: modern stateless requests (2026-07-28, per-request `_meta`, mirrored `Mcp-*` headers
  validated) and legacy `initialize`-handshake clients (2025-03-26..2025-11-25) on the same `/mcp` endpoint. Owns the
  tool list and the per-user bearer token (`FProtoBoyMCPConnectionStore`, `Saved/ProtoBoy/MCPConnection.json`). Server
  instructions are in `PBMCPServer.cpp`. Intended to move into the daemon later.
- `FProtoBoyMCPClientConfig` adds/removes ProtoBoy's entry in each client's config (JSON merge with a `.protoboy.bak`
  backup; Claude Code via the `claude mcp` CLI) and builds the manual-setup snippet; `FProtoBoyMCPServer::ConnectClient`
  is the one-click entry point. Team-wide options (port, write-tool gating, disabled tools) live in `UProtoBoySettings`.
  UI: `SProtoBoyConnectLLMDialog` and its panels in UI/MCP/.
- **Tools and prompts live with their feature:** `Docs/PBDocsMCP` (`locate_feature`, `docs_*`, `record_decision`;
  prompt `record-decision`), `Tasks/PBTasksMCP` (`tasks_list`, `task_*`; prompts `generate-tasks`, `implement-task`),
  `Sourcing/PBSourcingMCP`. The central registries only hold what spans features: `MCP/PBMCPTools.cpp`
  (`get_project_context`, then each feature's `RegisterTools`) and `MCP/PBMCPPrompts.cpp` (`plan-feature`, then each
  feature's `RegisterPrompts`). Each tool has an input schema and an async handler (call `Done` exactly once); schema,
  argument and subsystem helpers (`GetSubsystemOrFail`, `ArgOr`, `MakeTool`) are in `MCP/PBMCPToolHelpers.h`. Tools tied
  to a source use `FProtoBoyMCPTool::IsAvailable`.
- Write tools need `bAllowMCPWriteTools`; with `bConfirmMCPWriteTools` each call waits for Allow/Deny on an editor
  notification (`FProtoBoyMCPApprovals`, denied after 120 s). Every write is logged to
  `Saved/ProtoBoy/AgentActivity.jsonl` (`FProtoBoyAgentActivityLog`).
- Connect also writes a managed `agent-guide` block into the project's `CLAUDE.md` / `GEMINI.md`
  (`ProtoBoyAgentGuide`), the always-loaded workflow guidance.

## Asset sourcing (Sourcing/)

- `ProtoBoyAssetSourcing` (Sourcing/PBAssetSourcing.h): asset *sourcing* only. Each source in `Sourcing/<Source>/`
  (`ProtoBoyFab` in `Sourcing/Fab/`), MCP tools and prompts in `Private/Sourcing/PBSourcingMCP.cpp`, UI in
  `UI/Sourcing/`.
- Team settings (Asset Sourcing category) say whether agents should avoid AI-generated assets and which sources are
  enabled (Fab today); the style assets must have comes from Content docs. `DescribePolicyForAgents()` feeds
  `get_project_context` and the agent guide.
- **Sourcing setup:** `GetMissingSetup()` lists what the Asset Sourcing page's lock waits on: suggesting existing
  assets instead of AI-generated ones (`bPreferSourcedAssets`) and at least one enabled source (`HasEnabledSource()`;
  add new sources there). The Setup page's Asset Sourcing check is looser: done when `bPreferSourcedAssets` is off
  (allowing AI generation is a valid choice), and when it's on, only once a source is enabled.
- With `bPreferSourcedAssets` on, agents are told to suggest paid and free assets from the enabled sources, matched to
  the Content docs and the project's existing assets.
- **Sources and searches:** `GetSources()` is the list of supported sources (`FProtoBoyAssetSource`: name,
  description, icon, `IsEnabled`, `OpenSearch`); add a new source there. Searches are source-independent
  (`FProtoBoyAssetSearch`: query, `EProtoBoyAssetType` filters, free only, price range) and `OpenSearch` opens one on
  every enabled source; each source maps it to its own URL (`ProtoBoyFab::MakeSearch`, `GetListingType`).
- UI: `SProtoBoyAssetSourcingPanel` (Setup > Asset Sourcing: the policy and source checkboxes) and
  `SProtoBoyAssetSourcingPage` (the dashboard's Asset Sourcing page: **Sources**, every supported source and whether
  it's enabled, and **Search For Assets**, the query and filters; listing asset needs from Content docs is a TODO).
- **Agent search:** with a search context ticked (docs, project assets), Search runs the picked agent through
  `FProtoBoyAssetSearchRun` (Sourcing/PBAssetSearchRun.h) instead of opening the query as typed. The prompt carries the
  request and filters, the Game doc and every Content doc, and/or the /Game summary. The agent gets web search
  (`FProtoBoyAgentRun::FParams::bAllowWebSearch`, Claude Code's WebSearch; still no WebFetch) to find specific Fab
  listings, and answers with JSON suggestions (name, description, Content doc, a 2-6 keyword query, type keys from
  `GetAssetTypeKey`, and 1-3 listings each: url, title, note). Only real listing URLs are kept
  (`ProtoBoyFab::ParseListingUrl`), each once.
- **Listing previews:** once the agent is done, the run reads each listing's page once, like a link preview
  (`ProtoBoyFab::FetchListingPreview`: og:title, og:image; the price from the page's schema.org Product offer, and the
  price before a sale from its `startingPrice`), drops listings that 404 (URLs the agent got wrong) and listings whose
  price is outside the user's filters, and downloads the preview image. Tiles show "Free" or "From $X" (the cheapest
  license; Professional costs more), with "was $Y" during a sale. The page decodes images on a worker thread into small `FSlateDynamicImageBrush`
  thumbnails cached by listing URL; the tiles read the run's data through lambdas, so they fill in without rebuilds.
- UI: an **Agent Log** section (scrollable, fixed height) while it runs, then **Suggested Assets**: per suggestion,
  listing tiles (preview, title, the agent's note; click opens the listing) and More Like This (the suggestion's search
  on every enabled source). While the agent runs or previews load, the search inputs are locked and Search
  becomes Cancel. The page polls the run from the core ticker, not an active timer, since hidden dashboard
  pages don't tick. Prompt and raw output are in `Saved/ProtoBoy/AssetSearch/`. Images in the docs aren't passed to
  the agent yet (TODO).
- Fab has no public search API, so `fab_search` only builds a fab.com search URL and opens it in the user's browser.
  The only Fab pages ProtoBoy fetches are individual listing pages, once each, for their preview tags (see Listing
  previews). Never call Fab's internal endpoints (`/i/listings/...`) or crawl its search pages.
- Asset lifecycle tagging / Greybox Replacement Matrix and the Prototype-to-Production Viability Index are **removed for
  now**. Don't re-add them unless asked; the old stubs (`FProtoBoyAssetLifecycle`, `FProtoBoyHealthChecker`) are in git
  history.

## Health (Health/)

Documentation health checks: which docs may need updating after changes in revision control. Nothing is written to the
docs.

- `ProtoBoyDocHealth` (Health/PBDocHealth.h): the setting and the connection check. The team setting
  `bEnableDocHealthChecks` only takes effect while the editor is connected to revision control (`ISourceControlModule`;
  `IsRevisionControlConnected`, `OpenRevisionControlLogin` keeps the current provider).
- `ProtoBoyDocHealthChanges::Collect` (Health/PBDocHealthChanges.h) is the deterministic half: the project files
  changed since each doc on the map was last edited, and which of them each doc names.
  - A doc's "last edited" is its file's modified date (`FProtoBoyDocInfo::ModifiedUtc`), not its history: docs often
    live outside the repository (an Obsidian vault).
  - **Git only** for now (`IsProviderSupported`): `git` from the PATH, run blocking in the project folder. One
    `git log --since=<oldest doc>` for commits, `git diff HEAD` and `git ls-files --others` for uncommitted and
    untracked files (dated by the file on disk), all limited to Source, Content, Plugins and Config. Other providers
    are a TODO.
  - Files are kept newest first, so each doc's changes are the first `NumChanged` files. `Named` are the ones whose
    file name (also as a class name without its prefix) or folder path appears in the doc's text: a hint for the agent
    and the fallback result, not a verdict.
- `FProtoBoyDocHealthRun` (Health/PBDocHealthRun.h) is one check: `Collect`, then an `FProtoBoyAgentRun` whose prompt
  carries the changed files and each affected doc's text (at most 60 docs, the ones naming the most changed files
  first). The agent may read the files and answers `{"docs": [{id, confidence, reason, files}]}`; only docs it was
  given and files that really changed after that doc are kept. If nothing changed, no agent runs. If the agent gives
  no usable answer, the report lists the docs that name changed files, unjudged (`bAgentAnswered` false). Prompt and
  raw output are in `Saved/ProtoBoy/DocHealth/`.
- `ProtoBoyDocHealthReport` (Health/PBDocHealthReport.h): the last report, per user, in
  `Saved/ProtoBoy/DocHealth/LastReport.json`. A cancelled run saves nothing.
- UI (UI/Health/): `SProtoBoyDocHealthPanel`, the Setup page's optional Documentation Health Checks section;
  `SProtoBoyHealthPage`, the dashboard's Health page: agent picker and Run Health Checks (Cancel while running), the
  Agent Log, and Docs That May Need Updating (the last report, also after a restart; one card per doc with Open Doc).
  It owns the run and polls it from the core ticker, like the Asset Sourcing page. Locked until
  `ProtoBoyDocHealth::IsEnabled` (the setting is ticked and revision control is connected), not by the general setup.

## Daemon (Daemon/)

- `FProtoBoyDaemonClient`: link to the planned engine-agnostic daemon over local WebSocket. Heavy analysis (and the MCP
  server) is meant to move there eventually so the editor stays responsive.

## Dashboard and UI (UI/)

- `SProtoBoyDashboard`: a sidebar of pages (Setup, Documentation, Tasks, Asset Sourcing, Health) and the selected page; the
  open page is remembered per user (by index, so add new pages at the end). It owns the setup checks and the lock that
  keeps the feature pages disabled until setup is done (`GetMissingSetup`, `MakeSetupLock`, which takes the list of
  what's missing). Asset Sourcing also waits on the sourcing setup (`GetMissingSourcingSetup`); Health waits only on
  health checks being enabled. Feature pages are
  their own widgets in their feature's folder.
- Shared widget builders are in `UI/PBWidgets.h` (`ProtoBoyWidgets::MakeSection`, the collapsible section card);
  `UI/PBStyle` has the plugin's brushes, including `ProtoBoy.Logo` (Resources/Icons/ProtoBoy.svg, a 16px white icon
  in the editor's style) used by the dashboard tab and the Tools menu entry, and `ProtoBoy.Logo.Large` beside the
  sidebar title. The README's copies (white and black, picked by GitHub theme) are in `.github/images/`; regenerate
  them if the logo changes.
