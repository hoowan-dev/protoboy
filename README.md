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

All in **Tools > ProtoBoy Dashboard**:

- **Setup**: connect docs, tasks and agents, with a checklist of what's left.
- **Doc Map**: a graph of your docs by layer. Add, link, move and delete docs; start from a genre template, or let an
  agent generate the docs from your project (experimental).
- **Tasks**: your Trello board grouped by category (one per doc), synced with the doc map. Generate task ideas with an
  agent and pick the ones to add.
- **MCP server**: one-click setup for Claude Code, Claude Desktop and Gemini CLI.
- **Asset sourcing** (optional): agents search Fab using the style in your Content docs instead of generating assets.

## Getting started

**Requirements:** Unreal Engine 5.9 and a C++ project. Optional: [Obsidian](https://obsidian.md), Trello, and an agent
(Claude Desktop also needs Node.js).

1. Copy or symlink `Unreal/ProtoBoy` into your project's `Plugins/` folder, build, and open **Tools > ProtoBoy
   Dashboard**.
2. **Documentation:** Setup > **Connect to Documentation...**, pick your vault, and create the Game doc (or start from a
   template).
3. **Tasks** (optional): Setup > **Connect Task Board...** > Trello, then pick or create a board.
4. **Agents:** Setup > **MCP Settings...**, start the server, and click **Connect** for each agent.
5. Ask your agent to plan a small feature with the `plan-feature` prompt.

## Where things are stored

- **Shared:** team settings in `Config/DefaultProtoBoy.ini`, your docs in the vault, and a managed section in
  `CLAUDE.md` / `GEMINI.md`.
- **Per user:** connections, tokens and the agent activity log in `Saved/ProtoBoy/` (don't commit it).

## Known limitations

- Agents only get ProtoBoy's tools while the Unreal Editor is open.
- Moving a doc on the map doesn't move its file.
- Trello calls haven't been fully tested against live Trello.

## Roadmap

- Doc links and completeness notes on the map, and layer-aware MCP tools and write permissions.
- Alignment checks between the Tech docs and the real code.
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
