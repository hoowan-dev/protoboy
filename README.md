<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset=".github/images/logo-white.svg">
    <img src=".github/images/logo-black.svg" alt="ProtoBoy logo" width="96" height="96">
  </picture>
</p>

# ProtoBoy

**Build games, not slop.** ProtoBoy is a game-dev prototyping companion that keeps AI-assisted work aligned with your
game. It connects AI coding agents (Claude Code, Claude Desktop, Gemini CLI) to your design docs and task board, so new
features land in the right system and serve a stated goal.

The repo currently contains the Unreal Engine 5.9 editor plugin. Other engines and a standalone background service come
later.

> **Status: early development (0.1, beta).** Most pieces build and are wired together but haven't been tested end to end
> against live agents and services. Expect rough edges.

## How it works

```
 Agents (Claude Code, Gemini CLI, ...)
        |  MCP over http://127.0.0.1 (bearer token)
        v
 ProtoBoy MCP server (inside the Unreal Editor)
        |
        +--> Docs  -> Obsidian vault (or a project folder)
        +--> Tasks -> Trello board
```

- **Docs are the model.** A Game doc (pitch, pillars, core loop) sits on top, with Design, Tech and Content docs under
  it. Each doc's place is stored in its own frontmatter, and each ends with a Decisions section.
- **Agents read before they build.** MCP tools give agents the project context, find the doc a feature belongs to, and
  read and update docs and tasks. Workflow prompts (`plan-feature`, `implement-task`, ...) show up as slash commands.
- **You stay in control.** Agent writes are off by default, can require approval for each change, and are logged.

## Features

In **Tools > ProtoBoy Dashboard**, or from the **ProtoBoy** button in the editor's bottom status bar:

- **Setup**: connect docs, tasks and agents, with a checklist of what's left.
- **Doc Map**: a graph of your docs by layer. Add, link, move and delete docs; start from a genre template, or let an
  agent generate the docs from your project (experimental).
- **Tasks**: your Trello board grouped by category (one per doc), synced with the doc map. Generate task ideas with an
  agent and pick the ones to add.
- **MCP server**: one-click setup for Claude Code, Claude Desktop and Gemini CLI.
- **Asset sourcing** (optional): agents search Fab using the style in your Content docs instead of generating assets.
- **Health**: find the docs that have fallen behind the project.
  - *Against revision control* (optional, Git): ProtoBoy lists the files changed since each doc was last edited, and
    an agent judges which docs those changes outdate. Open a flagged doc to fix it, or mark it as up to date.
  - *Structure* (no agent needed): links to notes that don't exist, project paths that don't exist, sections nobody
    has written, Design docs with no Tech or Content doc, and parts of the project no doc mentions.
- **Status bar button**: a ProtoBoy button beside Revision Control in every editor window. Its icon shows where the
  docs stand (checking, needs attention, healthy, not checked); its menu jumps to any dashboard page, runs the health
  checks in the background and lists the docs that need updating.

## Getting started

**Requirements:** Unreal Engine 5.9 and a C++ project. Optional: [Obsidian](https://obsidian.md), Trello, an agent
(Claude Desktop also needs Node.js), and Git for the health checks against revision control.

1. Copy or symlink `Unreal/ProtoBoy` into your project's `Plugins/` folder, build, and open **Tools > ProtoBoy
   Dashboard**.
2. **Documentation:** Setup > **Connect to Documentation...**, pick your vault, and create the Game doc (or start from a
   template).
3. **Tasks** (optional): Setup > **Connect Task Board...** > Trello, then pick or create a board.
4. **Agents:** Setup > **MCP Settings...**, start the server, and click **Connect** for each agent.
5. Ask your agent to plan a small feature with the `plan-feature` prompt.
6. **Health** (optional): connect the editor to revision control, tick **Enable documentation health checks** under
   Setup > Documentation Health Checks, then click the **ProtoBoy** button in the status bar > **Run Health Checks**.
   The structure checks work without this step.

## Where things are stored

- **Shared:** team settings in `Config/DefaultProtoBoy.ini`, your docs in the vault, and a managed section in
  `CLAUDE.md` / `GEMINI.md`. Marking a doc as up to date adds a `protoboy-reviewed` date to its frontmatter; the health
  checks write nothing else to your docs.
- **Per user:** connections, tokens, the agent activity log and the last health report in `Saved/ProtoBoy/` (don't
  commit it).

## Known limitations

- Agents only get ProtoBoy's tools while the Unreal Editor is open.
- Moving a doc on the map doesn't move its file.
- Trello calls haven't been fully tested against live Trello.
- Health checks against revision control only read Git history (Perforce and the others aren't supported yet), and
  need `git` on your PATH.
- A doc counts as "last edited" by its file date, so moving it on the map makes it look freshly edited, and commits
  you pulled after editing a doc can be missed.
- The structure checks only verify file and folder paths, not class or asset names.

## Roadmap

- Doc links and completeness notes on the map, and layer-aware MCP tools and write permissions.
- Deeper health checks: class and asset names that no longer exist, contradictions between docs, and task board
  checks (cards linking to deleted docs, finished work that was never documented).
- Health checks for other revision control providers, and as an MCP tool so agents can ask what's out of date.
- A standalone daemon, so agents keep context without the editor open.
- More providers (Notion, Confluence, Jira, Linear, GitHub Issues) and asset sources.
- Unity and Godot plugins.

## For contributors

- Architecture: [Unreal/ProtoBoy/ARCHITECTURE.md](Unreal/ProtoBoy/ARCHITECTURE.md)
- Build and coding rules: [CLAUDE.md](CLAUDE.md)

Build the plugin on its own (package outside the repo):

```
"C:\Program Files\Epic Games\UE_5.9\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="%CD%\Unreal\ProtoBoy\ProtoBoy.uplugin" -Package="%TEMP%\ProtoBoyBuild" -TargetPlatforms=Win64
```
