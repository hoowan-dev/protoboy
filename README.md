# ProtoBoy

**Build games, not slop.** ProtoBoy is a game-dev prototyping companion that keeps AI-assisted work aligned with your
game's architecture. It connects AI coding agents (Claude Code, Claude Desktop, Gemini CLI) to your project's
architecture doc, design docs and task board, so new features land in the right system, follow the project's rules,
and serve a stated end goal instead of piling into monolithic, goal-less code.

The repo currently contains the Unreal Engine 5.9 editor plugin. Other engines and a standalone background service
come later (see [Roadmap](#roadmap)).

> **Status: early development (0.1, beta).** The pieces below build and are wired together, but most have not yet been
> tested end to end against live agents and services. Expect rough edges.

## How it works

```
 Agents (Claude Code, Gemini CLI, ...)
        |  MCP over http://127.0.0.1 (bearer token)
        v
 ProtoBoy MCP server (inside the Unreal Editor)
   tools:   project context, feature placement, docs, decisions, tasks
   prompts: plan-feature, implement-task, record-decision
        |
        +--> Docs provider  -> Obsidian vault (or a project folder)
        |                      Game.md, Design/, Tech/, Content/
        +--> Task provider  -> Trello board (To Do / In Progress / Done)
```

1. **The model.** The project's docs in four layers: the Game doc (pitch, design pillars, non-goals, core loop),
   Design docs per feature with paired Tech docs, shared Tech docs, and Content docs, each ending with its Decisions;
   plus tasks that link to the Design doc they implement. Each doc's layer is in its own frontmatter (see
   [Doc map](#doc-map)).
2. **Tools.** Agents call `get_project_context` before designing (the Game doc and the doc map), `locate_feature` to
   find the Design or shared Tech doc new work belongs to, and docs and task tools to read and update both.
3. **Workflow.** The guidance agents follow comes from three places: a managed section ProtoBoy writes into the
   project's `CLAUDE.md` / `GEMINI.md`, the MCP server's instructions, and workflow prompts that show up as slash
   commands. A new feature or system the docs don't cover needs your agreement first, recorded as a decision.

## Features

### Dashboard

**Tools > ProtoBoy Dashboard** has four pages, picked from the list in its sidebar (the dashboard reopens on the last
page you used):

- **Setup**: a shortcut to the plugin's Project Settings, plus the **MCP**, **Documentation**, **Tasks** and optional
  **Asset Sourcing** sections. Each shows its connection status and what's left to set up, with a check or a red X in
  its header (hover it for details). These are the same checks as on the Architecture page.
- **Architecture**: two sections.
  - **Setup Checks**: a check or warning for connecting an agent (MCP), documentation (Obsidian, plus the Game doc) and a task board (Trello, with lists mapped). Each has a **Set Up...** shortcut until it's done. A fourth check,
    **Asset sourcing**, is always green because sourcing is optional. It shows the team's choices: whether agents source
    assets instead of generating them, the enabled sources, and the art direction note. Its **Change...** button opens the
    Setup page.
  - **Doc Map**: the project's docs by layer (see [Doc map](#doc-map)). It's disabled until an agent, documentation and a task board are connected (a missing Game doc doesn't lock it, since this is where you create it).
- **Documentation** and **Tasks**: blank for now.

### MCP server and one-click agent setup

- **The server.** A local MCP server (Streamable HTTP) runs in the editor on `127.0.0.1` only. Every request needs a
  per-user bearer token, and browser origins other than localhost are rejected. It supports the current stateless MCP
  revision (2026-07-28) as well as older clients that use the `initialize` handshake.
- **Connect.** One click sets up an agent:
  - **Claude Code**: added for this project only, through the `claude mcp` CLI.
  - **Claude Desktop**: bridged through `mcp-remote`, which needs Node.js.
  - **Gemini CLI**: added to your user settings.

  ProtoBoy only edits its own entry, backs up JSON configs first, and offers a copy-and-paste fallback. Regenerating
  the token updates every connected agent.
- **Status.** The dashboard lists the agents that have connected recently.

**Tools**

| Read | Write |
|---|---|
| `get_project_context`, `locate_feature`, `docs_list`, `docs_search`, `docs_read`, `tasks_list`, `task_get`, `fab_search` (when Fab is enabled) | `docs_create`, `docs_update_section`, `record_decision`, `task_create`, `task_update_status`, `task_comment` |

**Prompts:** `plan-feature`, `implement-task`, `record-decision`, `source-assets`.

**Write safety**

- Write tools are off until you allow them in **MCP Settings > Server**.
- With **Ask before each change** on, every write waits for **Allow / Deny** on an editor notification. Unanswered
  requests are denied after 2 minutes.
- Docs are never rewritten: agents can only append to a section, create new files, or replace blocks ProtoBoy manages.
  They can only append to the Game doc.
- Every write, including denied ones, is logged to `Saved/ProtoBoy/AgentActivity.jsonl`.

### Documentation (Obsidian)

- **Connecting.** Pick a vault (ProtoBoy reads the vaults Obsidian knows about) and a docs folder inside it. ProtoBoy
  writes plain Markdown there, so Obsidian picks up changes live.
- **The Game doc.** Once connected, set it up from a template (with a starting set of docs), create it on its own, or
  give an existing note the `protoboy-layer: game` property. Agents read it before designing anything. You can open it
  in Obsidian from the setup window.
- **Fallback.** Without Obsidian, the agent tools fall back to a project folder (`DocsDirectory`, default
  `Docs/ProtoBoy`).

### Tasks (Trello)

- **Connecting.** One click: Trello's "Allow" page opens in your browser and redirects back to ProtoBoy on localhost.
  The token is checked with Trello and stored per user.
- **The project board.** Pick a board and map its **To Do / In Progress / Done** lists (guessed from list names), or
  create a board from ProtoBoy's template. If a board is missing lists, **Add Missing Lists** creates only those.
- **Scope.** Agents can only see and change cards on the configured board.

Trello needs an API key from a Trello Power-Up. The Trello page of **Connect Task Board** walks you through creating
one and adding ProtoBoy's local address under **Allowed origins**.

### Doc map

The project's docs in four layers, from why the game exists down to how each piece is built. It's on the
**Architecture > Doc Map** page:

```
Game             why the game exists, and what it is     1 doc: pitch, pillars, core loop, an index of the rest
  ├ Design       what each feature is (player view)      one per feature, nestable
  │   └ Tech     how that feature is built               pairs with its Design doc and mirrors its sections
  ├ Tech         shared tech with no player face         save/load, input, networking, the AI framework
  └ Content      what's in the game, and how it reads    levels, items, characters; art, audio, UX, writing style
```

- **The docs are the model.** Each doc says which layer it's in through a few properties in its frontmatter (what
  Obsidian shows as note properties): `protoboy-id`, `protoboy-layer`, and when needed `protoboy-parent` (the doc it
  sits under) and `protoboy-implements` (the Design doc a Tech doc implements). There's no separate file to keep in
  sync: ProtoBoy builds the map by reading the docs, and anything it changes is written into them. Edits made in
  Obsidian show up after **Refresh**.
- **Where docs sit.** The Game doc is the top. Without a `protoboy-parent`, a feature's Tech doc sits under the Design
  doc it implements, and everything else under the Game doc. Nothing restricts which layer goes under which.
- **Decisions live in the docs.** Every doc ends with a **Decisions** section: dated entries for what was decided there,
  why, and what it rules out. Agents add entries with `record_decision`, in the doc the decision affects (the Game doc
  for scope, pillars and project-wide conventions).
- **Templates.** New docs start with their template's sections. The hints under each heading are HTML comments, so
  they're hidden when reading in Obsidian and ignored when ProtoBoy reads the doc. Every template ends with Decisions.

  | Doc | Sections |
  |---|---|
  | **Game** | Pitch · Player Fantasy · Design Pillars (3-5, one `###` each, with the feeling it gives and what it rules out) · Non-goals · Core Loop · Game Flow · Areas (generated) · Glossary |
  | **Design** | Purpose (which pillars it serves) · Player Experience · Rules · What the Player Sees · Edge Cases · Open Questions |
  | **Tech** (a feature's) | The same sections as its Design doc, each answered from the engine side |
  | **Tech** (shared) | Responsibilities · Interface · Data · Used By |
  | **Content** | Inventory · Authoring Rules · Where the Data Lives · Fed Into |
  | **Content** (style guide) | Goals · Style · References · Budgets |

- **Set Up from Template.** A window that lists preset doc sets, by genre (Action / Platformer, RPG, Shooter, Strategy /
  Management, Puzzle, Narrative / Adventure, Survival / Crafting, Roguelike) and by size (Game Jam, Prototype / Vertical
  Slice, Multiplayer, Live / Production), plus Generic. Each preview shows the template's suggested design pillars and its
  docs as a tree. **Next** lists exactly what will be written and what's already there; applying creates only the docs
  that don't exist yet and never overwrites one. A new Game doc gets the template's pillars as placeholders to rewrite,
  and each new doc starts from its template's sections with a line on what it covers. Templates are JSON files in
  `Unreal/ProtoBoy/Resources/DocTemplates/`. The button is on the doc map's toolbar, next to **Create Game Doc** when
  the map is empty, and on the Obsidian page of **Connect to Documentation**.
- **The Game doc first.** **Create Game Doc** writes `Game.md` at the top of the docs folder. The documentation setup
  check needs it. Its **Areas** section is a managed block that ProtoBoy keeps listing every doc below it, grouped by
  layer; write anything else around it.
- **Adding docs.** Type a title, then **Add Doc** and pick **Design Doc**, **Shared Tech Doc**, **Content Doc** or
  **Style Guide**; the doc goes under the selected one (or the Game doc). Design docs nest in folders the way features do
  (`Design/Combat/Weapons/Weapons.md`); shared tech goes in `Tech/`, content and style guides in `Content/`. You can also
  drag off a node's bottom bar into empty space, or right-click a node.
- **Design and Tech pairs.** **Add Tech Doc** creates the selected Design doc's Tech doc next to it
  (`Combat.tech.md`), with one section for each `##` section of the Design doc, so every design question gets its
  engine-side answer. The details pane shows each Design doc's Tech doc, or says it has none.
- **Existing notes.** **Map Existing Doc** lists the notes in the docs folder that aren't on the map and gives one a
  layer. Only its frontmatter changes. Adding the properties by hand in Obsidian works too.
- **Docs from the eight-layer version** stay on the map without changes: a Vision and an Overview are both Game docs
  (the Vision is the top, the Overview sits under it), Systems docs are shared Tech, Direction docs are Content. ADRs
  stay in `ADR/` as files but aren't on the map.
- **Moving docs.** Wire a doc's bottom bar to another doc's top bar to put the second under the first. This writes
  `protoboy-parent` into the doc; the file itself doesn't move. Unhooking a doc sends it back to its layer's default
  place.
- **The graph.** Behavior Tree style, colored by layer, with each doc's first written line on its node. Double-click
  opens a doc (in Obsidian when connected). The details pane shows a doc's layer, file, the doc it's under, its Tech or
  Design doc, the docs below it, and for the Game doc, the design pillars it lists.

### Asset sourcing (optional)

Stops agents from defaulting to AI-generated assets. In **Setup > Asset Sourcing** you choose:

- **Policy.** Whether agents should prefer sourced assets (on by default). Agents see the policy in
  `get_project_context` and in the agent guide.
- **Sources.** Which ones are enabled. Today that's **Fab**: agents call `fab_search` with terms derived from the
  design doc and art direction, ProtoBoy opens the fab.com search in your browser, and you pick the listings. Fab has
  no public search API, so ProtoBoy never reads Fab results itself.
- **Art direction.** An optional art direction doc that agents read before choosing assets.

## Getting started

### Requirements

- Unreal Engine 5.9, and a C++ project to host the plugin.
- Optional: [Obsidian](https://obsidian.md), a Trello account, and one or more agents (Claude Code, Claude Desktop,
  Gemini CLI). Claude Desktop also needs Node.js for `mcp-remote`.

### Install

Copy or symlink `Unreal/ProtoBoy` into your project's `Plugins/` folder and build the project. Open
**Tools > ProtoBoy Dashboard**.

### First-time setup

1. **Documentation:** Setup > Documentation > **Connect to Documentation...**. Connect your vault, then set up the
   Game doc (from a template, or on its own) and fill in at least the pitch and the design pillars. Add Design docs from **Architecture >
   Doc Map**.
2. **Tasks** (optional): Setup > Tasks > **Connect Task Board...** > Trello. Set up an API key, click **Connect**, then
   pick or create the project board.
3. **Agents:** Setup > MCP > **MCP Settings...**. Start the server, allow agent changes if you want them, then click
   **Connect** for each agent you use and restart that agent.
4. **Try it:** ask your agent to plan a small feature, for example with the `plan-feature` prompt.

## Where things are stored

| What | Where | Shared? |
|---|---|---|
| Team settings (ports, board and list mapping, write gating, Trello API key) | `Config/DefaultProtoBoy.ini` | Yes, committed |
| Docs, and each doc's place on the doc map (its frontmatter) | The docs folder (Obsidian vault, or `DocsDirectory`) | With your vault |
| Agent guide section | `CLAUDE.md`, `GEMINI.md` in the project root (managed block only) | Yes, committed |
| Your docs connection | `Saved/ProtoBoy/DocsConnection.json` | No |
| Your MCP token and connected agents | `Saved/ProtoBoy/MCPConnection.json` | No |
| Your Trello token | `Saved/ProtoBoy/TasksConnection.json` | No |
| Agent write log | `Saved/ProtoBoy/AgentActivity.jsonl` | No |

Tokens are stored in plain text in `Saved/`, which shouldn't be committed. A Trello API key isn't a secret; the
Power-Up's secret is, and ProtoBoy never needs it.

## Known limitations

- **Editor only.** Agents only get ProtoBoy's tools while the Unreal Editor is open with the MCP server running.
- **`locate_feature` is a hint.** It matches keywords against the Design and shared Tech docs, so treat its answer as a
  suggestion rather than a decision.
- **Moving a doc on the map doesn't move its file.** Its place is stored in its frontmatter; renaming or moving files
  is done in Obsidian.
- **Stopping doesn't free the ports.** Stopping the MCP server (or finishing a Trello connect) leaves the port bound,
  because the engine can't close a single listener. It answers "not found" until the next start.
- **Untested Trello calls.** The board, list and card requests haven't been tested against live Trello yet.
- **Claude Code setup briefly freezes the editor.** Connecting Claude Code runs the `claude` CLI on the editor's main
  thread for a second or two.

## Direction: the doc layers

> Partly built: the four layers, the doc map, the Game doc, Design and Tech pairs, Decisions sections and doc-set
> templates are in. The rest is the direction; some decisions are still open.

Large teams document a game in layers: a short vision, a master doc that indexes everything, a doc per feature written
from the player's side, a technical spec that mirrors it, and content and style docs alongside. Every feature traces
back to the design pillars, the handful of experience goals used to decide what gets in. ProtoBoy keeps that to four
layers (Game, Design, Tech, Content), maps them so agents read them in order before they build, and keeps the technical
layer current as they work.

- **Links.** Beyond where a doc sits: Design docs say which pillars they serve, Content docs which features they feed,
  and Design and Tech docs which shared tech they use. Each is a frontmatter property, shown in a managed header block
  in the doc. A Design doc that serves no pillar gets a quiet note.
- **Completeness notes.** The map shows what's missing without enforcing it: an empty section, a Design doc without a
  Tech doc, a Tech section the Design doc no longer has.
- **Agents read down the layers.** Before writing code, an agent reads the Game doc, the Design doc and its Tech doc.
  Tools: `get_doc_map`, `find_doc_for(topic)`, `get_game`.
- **Write permissions by layer.** Agents may write Tech and Content docs (inventories and data, not style rules), only
  add Open Questions and proposed decisions to Design docs, and never write the Game doc. The design stays in human
  hands; the technical docs stay current.

ProtoBoy can't guarantee an agent stays aligned. The goal is to make the aligned path the easy one, and misalignment
visible.

## Roadmap

**Next: the doc layers**

1. ~~Layers in frontmatter, and the map read from the docs.~~ Done.
2. ~~The Game doc with its sections, replacing the Architecture doc.~~ Done.
3. ~~Design and Tech pairs, and Add Tech Doc.~~ Done.
4. Links (serves, fed into, uses) and completeness notes on the map.
5. MCP tools for the map (`get_doc_map`, `find_doc_for`, `get_game`) and agent write permissions by layer.
6. ~~Doc-set templates (Set Up from Template).~~ Done. Still to do: migrating projects that used the pillar tree.
7. Retrieval and checks: ranked search filtered by layer and doc, an alignment check before code is written, and
   deterministic checks that compare the project's real structure (modules, folders, key classes) with the Tech docs.
8. Move the MCP server and the docs and task connectors into a standalone **daemon**, so agents keep project context
   without the editor open, one service can support every engine, and optional local embeddings have a home.

**From the concept**

- Living docs: TDD stubs generated when gameplay assets or classes are created, regenerating only managed blocks.
- More docs and task platforms behind the same provider interfaces: Notion and Confluence; Jira, Basecamp, Linear and
  GitHub Issues.
- More asset sources behind the Asset Sourcing section: Kenney, Poly Haven, ambientCG, with license metadata.
- Toolchain: Git and Perforce hooks, and turning `TODO` / `HACK` markers into tracked tasks.
- Test scaffolding for mechanics (Unreal Automation Spec, Unity Test Framework, GdUnit4).
- Unity and Godot plugins.

**Shelved for now:** project health checks with the Prototype-to-Production Viability Index, and asset lifecycle
tagging with the Greybox Replacement Matrix. The earlier stubs are in git history.

## Repository layout

```
Unreal/ProtoBoy/                 Unreal Engine 5.9 editor plugin
  Resources/Icons/               Provider logos (SVG)
  Resources/DocTemplates/        Doc-set templates for Set Up from Template (JSON)
  Source/ProtoBoy/
    Public/ & Private/
      PBModule, PBSubsystem      Module startup, dashboard tab, editor subsystem that owns the services
      PBSettings                 Project Settings > Plugins > ProtoBoy
      Docs/                      Doc layers and the doc map, docs service, Markdown helpers, docs provider interface
        Obsidian/                Obsidian vault connector and provider
      Tasks/                     Task provider interface, task board connection store
        Trello/                  Trello connect flow and REST client
      Sourcing/                  Asset sourcing policy, its MCP tools and prompts
        Fab/                     Fab source (opens fab.com searches)
      MCP/                       MCP server, tools, prompts, approvals, agent guide, client config
      Net/                       Shared HTTP helpers
      Daemon/                    Client for the future daemon (stub)
      UI/                        Dashboard and style; each feature's windows and panels in UI/<Feature>/
```

Unity, Godot and the daemon will get their own top-level folders (`Unity/`, `Godot/`, `Daemon/`).

## Building the plugin on its own

```
"C:\Program Files\Epic Games\UE_5.9\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="%CD%\Unreal\ProtoBoy\ProtoBoy.uplugin" -Package="%TEMP%\ProtoBoyBuild" -TargetPlatforms=Win64
```

Package outside the repo. A successful build ends with `BUILD SUCCESSFUL` and produces `UnrealEditor-ProtoBoy.dll` under
`Binaries/Win64/` in the package folder. Contributor notes (unity-build rules, engine API checks, architecture) are in
[CLAUDE.md](CLAUDE.md).
