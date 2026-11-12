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
   tools:   project context, feature placement, docs, ADRs, tasks
   prompts: plan-feature, implement-task, record-decision
        |
        +--> Docs provider  -> Obsidian vault (or a project folder)
        |                      Architecture doc, Design/, ADR/
        +--> Task provider  -> Trello board (To Do / In Progress / Done)
```

1. **The model.** An Architecture doc (end goal, current milestone, systems, dependency rules, conventions), per-feature
   design docs, ADRs, and tasks that link to the design doc they implement.
2. **Tools.** Agents call `get_project_context` before designing, `locate_feature` to find the system that owns new
   work, and docs and task tools to read and update both.
3. **Workflow.** The guidance agents follow comes from three places: a managed section ProtoBoy writes into the
   project's `CLAUDE.md` / `GEMINI.md`, the MCP server's instructions, and workflow prompts that show up as slash
   commands. A new system, or an exception to a rule, needs an ADR and your agreement first.

## Features

### Dashboard

**Tools > ProtoBoy Dashboard** has two sections:

- **Setup**: a shortcut to the plugin's Project Settings, plus the **MCP**, **Documentation**, **Tasks** and optional
  **Asset Sourcing** sections. Each shows its connection status and what's left to set up.
- **Planning**: a setup checklist, with a check or warning for connecting an agent (MCP), documentation (Obsidian and
  the Architecture doc) and a task board (Trello, with lists mapped). Each step has a **Set Up...** shortcut until it's
  done. Below it is the **pillar tree** editor (see [Pillars](#pillars)).

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
| `get_project_context`, `locate_feature`, `docs_list`, `docs_search`, `docs_read`, `tasks_list`, `task_get`, `fab_search` (when Fab is enabled) | `docs_create`, `docs_update_section`, `adr_create`, `task_create`, `task_update_status`, `task_comment` |

**Prompts:** `plan-feature`, `implement-task`, `record-decision`, `source-assets`.

**Write safety**

- Write tools are off until you allow them in **MCP Settings > Server**.
- With **Ask before each change** on, every write waits for **Allow / Deny** on an editor notification. Unanswered
  requests are denied after 2 minutes.
- Docs are never rewritten: agents can only append to a section, create new files, or replace blocks ProtoBoy manages.
  They can only append to the Architecture doc.
- Every write, including denied ones, is logged to `Saved/ProtoBoy/AgentActivity.jsonl`.

### Documentation (Obsidian)

- **Connecting.** Pick a vault (ProtoBoy reads the vaults Obsidian knows about) and a docs folder inside it. ProtoBoy
  writes plain Markdown there, so Obsidian picks up changes live.
- **The Architecture doc.** Create it from a template, or point the setting at an existing note. Its path is up to you:
  any name or folder inside the docs folder, default `Architecture.md`. Agents read it before placing work. You can
  open it in Obsidian from the setup window.
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

### Pillars

The team's architecture as a tree of **pillars**: mechanics, domains, disciplines, or whatever fits the game, nested as
deep as needed. Edit it in **Planning > Pillars**:

- **Tree editor:** add, nest, reorder and remove pillars, or start from a common set (Gameplay, Animation, Art, Audio,
  Content, UI). Each pillar has a name, a free-form kind, a one-line vision, a parent and the pillars it depends on.
- **Stable IDs:** each pillar keeps its ID when renamed or moved, so docs and (later) tasks keep pointing at it.
- **Saved to a file:** `ProtoBoy/Pillars.json` in the project (configurable), meant to be committed and shared.
- **Generate Docs...:** previews, then writes, a doc per pillar under `Pillars/` in the docs folder, nested like the
  tree. Each doc starts from a template (Vision, Goals, Non-goals, Design, Rules, Interfaces, Art & assets, Open
  questions). ProtoBoy only owns a header block with the kind, vision, and links to the parent, children and
  dependencies (Obsidian wikilinks, so the graph mirrors the tree). The Architecture doc gets a generated pillar
  overview. Renamed pillars keep their doc; removed ones have their doc marked archived, never deleted.

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

1. **Documentation:** Setup > Documentation > **Connect to Documentation...**. Connect your vault, then create the
   Architecture doc (or point to an existing one) and fill in at least the end goal and a few systems.
2. **Tasks** (optional): Setup > Tasks > **Connect Task Board...** > Trello. Set up an API key, click **Connect**, then
   pick or create the project board.
3. **Agents:** Setup > MCP > **MCP Settings...**. Start the server, allow agent changes if you want them, then click
   **Connect** for each agent you use and restart that agent.
4. **Try it:** ask your agent to plan a small feature, for example with the `plan-feature` prompt.

## Where things are stored

| What | Where | Shared? |
|---|---|---|
| Team settings (ports, board and list mapping, Architecture doc path, write gating, Trello API key) | `Config/DefaultProtoBoy.ini` | Yes, committed |
| Pillar tree | `ProtoBoy/Pillars.json` (configurable) | Yes, committed |
| Agent guide section | `CLAUDE.md`, `GEMINI.md` in the project root (managed block only) | Yes, committed |
| Your docs connection | `Saved/ProtoBoy/DocsConnection.json` | No |
| Your MCP token and connected agents | `Saved/ProtoBoy/MCPConnection.json` | No |
| Your Trello token | `Saved/ProtoBoy/TasksConnection.json` | No |
| Agent write log | `Saved/ProtoBoy/AgentActivity.jsonl` | No |

Tokens are stored in plain text in `Saved/`, which shouldn't be committed. A Trello API key isn't a secret; the
Power-Up's secret is, and ProtoBoy never needs it.

## Known limitations

- **Editor only.** Agents only get ProtoBoy's tools while the Unreal Editor is open with the MCP server running.
- **`locate_feature` is a hint.** It matches keywords against the Architecture doc's systems, so treat its answer as a
  suggestion rather than a decision.
- **Stopping doesn't free the ports.** Stopping the MCP server (or finishing a Trello connect) leaves the port bound,
  because the engine can't close a single listener. It answers "not found" until the next start.
- **Untested Trello calls.** The board, list and card requests haven't been tested against live Trello yet.
- **Claude Code setup briefly freezes the editor.** Connecting Claude Code runs the `claude` CLI on the editor's main
  thread for a second or two.

## Vision: the pillar tree

> Partly built: the pillar tree and doc generation (step 1 below) are in. The rest is the direction; some decisions
> are still open.

Today, the Architecture doc's list of systems is what anchors agents. The plan is to replace it with a **tree of
architectural pillars** that the team defines: gameplay, animation, art, content, or whatever fits the game. Pillars
can be organised around mechanics, domains or disciplines, and nest as deep as needed (for example
`gameplay/combat/abilities`). Everything else hangs off that tree.

**Author once, generate the rest.** You build the tree in the dashboard's **Planning** section. ProtoBoy then
generates matching structure:

- **Docs:** a doc per pillar, for example `Pillars/Gameplay/Combat.md`, from a template: Vision, Goals, Non-goals,
  Design, Rules, Interfaces, Art & assets. ProtoBoy only owns a small managed header (parent and child links,
  dependencies), so the rest is yours. In Obsidian the links mirror the tree in the graph view.
- **Tasks:** pillar labels on the board, plus a `Pillar: gameplay/combat` line on each card.
- **Sync:** generation is previewed before it's applied, and follows stable pillar IDs through renames. Removing a
  pillar archives its doc and label rather than deleting anything a person wrote.

**You or the agent fill it in.** Pillars start as outlines. You can write the vision yourself, or let an agent
interview you about the game and each pillar and draft the sections for you to approve.

**Alignment comes from retrieval plus checks.** Retrieval puts the right context in front of the agent; checks catch
what slips through.

- **Structural retrieval first.** For a task or feature, ProtoBoy resolves its pillar and hands the agent that pillar's
  doc and rules, the vision of its parent pillars, the interfaces of the pillars it depends on, and the art direction.
  It's predictable and easy to explain, which matters more at game-project scale than fuzzy matching.
- **Search for everything else.** Ranked keyword search across the docs, filterable by pillar. Semantic search with
  local embeddings may follow, opt-in and running in the daemon, since ProtoBoy deliberately holds no LLM keys.
- **Gates:**
  - Every task and design change names its pillar. Work that fits no pillar needs an ADR proposing one.
  - An alignment check gives the agent the relevant vision, rules and a checklist to answer before it writes code.
  - Deterministic checks flag code outside its pillar's folders, and dependencies between pillars that break the rules.
  - Asset suggestions use the pillar's art notes and the project's art direction, so prototypes fit the intended look.

ProtoBoy can't guarantee an agent stays aligned. The goal is to make the aligned path the easy one, and misalignment
visible and checked.

## Roadmap

**Next: the pillar tree**

1. ~~Pillar model and editor: a committed pillar file with stable IDs, a tree editor in **Planning**, and doc
   generation with a preview.~~ Done.
2. Pillar-aware agents: a `get_context` tool for a task or pillar, a required pillar on task and design writes, and a
   "define the vision" interview prompt.
3. Task projection: pillar labels and `Pillar:` lines on the board, with a preview.
4. Retrieval and gates: ranked search filtered by pillar, and an alignment check before code is written.
5. Deterministic checks: read the project's real structure (modules, plugins, folders, key classes), compare it with
   the pillar tree, and report drift. Per-pillar asset alignment.
6. Move the MCP server and the docs and task connectors into a standalone **daemon**, so agents keep project context
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
  Source/ProtoBoy/
    Public/ & Private/
      PBModule, PBSubsystem      Module startup, dashboard tab, editor subsystem that owns the services
      PBSettings                 Project Settings > Plugins > ProtoBoy
      Docs/                      Docs service, Markdown helpers, docs provider interface
        Obsidian/                Obsidian vault connector and provider
      Tasks/                     Task provider interface, task board connection store
        Trello/                  Trello connect flow and REST client
      Planning/                  Pillar tree and pillar doc generation
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
