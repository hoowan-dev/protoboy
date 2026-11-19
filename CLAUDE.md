# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

ProtoBoy is a game-dev prototyping companion: living docs, asset lifecycle tagging, project health checks, and toolchain integration (the plugin currently implements docs, MCP and task boards). The full concept and roadmap are in the Obsidian vault at `C:\Users\becer\Documents\Obsidian\Hoowan Vault\Code\ProtoBoy\ProtoBoy Ideas.md`. Read it before adding features: the code is organized by the "pillars" defined there.

The repo currently contains only the Unreal Engine 5.9 editor plugin (`Unreal/ProtoBoy`). It is intentionally a base: most functions are stubs marked `// TODO` that describe the intended behavior. Keep new work simple and stub-first unless asked to implement something fully. Future engines and the daemon go in sibling top-level folders (`Unity/`, `Godot/`, `Daemon/`).

**Architecture details are in [Unreal/ProtoBoy/ARCHITECTURE.md](Unreal/ProtoBoy/ARCHITECTURE.md)**, one section per area. Read the section for the area you're changing, and keep it up to date when you change how something works.

## Build

UE 5.9 is installed at `C:\Program Files\Epic Games\UE_5.9`. There is no host project in the repo; build the plugin on its own with BuildPlugin. Package to a temp/scratch folder, never inside the repo:

```
"C:\Program Files\Epic Games\UE_5.9\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="C:\dev\protoboy\Unreal\ProtoBoy\ProtoBoy.uplugin" -Package="<temp dir>" -TargetPlatforms=Win64
```

- Do **not** pass `-NoHostPlatform`. It skips the editor target, so the editor-only module never compiles and the build "succeeds" without checking anything.
- Success means `BUILD SUCCESSFUL` and `UnrealEditor-ProtoBoy.dll` in `<temp dir>/Binaries/Win64/`. When filtering output, grep for `error|warning C|BUILD SUCCESSFUL|ExitCode`.
- Check engine APIs against the headers under `Engine/Source` rather than from memory, since 5.9 may differ from older versions.
- **Unity builds:** the editor merges the module's .cpp files into `Module.ProtoBoy.cpp`, so file-local names collide across files. Never put `using namespace` at file scope, and don't use anonymous namespaces for helpers: put them in a namespace named after the file (e.g. `ProtoBoyTrello`) and call them qualified. Helpers shared by several files go in a shared header (e.g. `Private/Net/PBHttpServerUtils.h`, `Private/Docs/PBDocMapUtils.h`).
- BuildPlugin does **not** catch unity collisions: adaptive unity compiles files modified in git separately. To check, copy the plugin into a scratch host project (`<scratch>/Host/Plugins/ProtoBoy` plus a minimal `.uproject` enabling it) and run `"C:\Program Files\Epic Games\UE_5.9\Engine\Build\BatchFiles\Build.bat" UnrealEditor Win64 Development -Project="<scratch>/Host/Host.uproject" -DisableAdaptiveUnity`; the output should show a single `Module.ProtoBoy.cpp`. It can't run while the editor has Live Coding active.
- **`FString::Find` clamps its start position** to the last character, so searching from `Len()` finds a trailing match again. In line loops use `ProtoBoyMarkdown::Detail::FindLineEnd` (Docs/PBMarkdown.cpp) or stop at `Len()` before calling `Find`; otherwise any doc ending in a newline loops forever (this froze the editor at startup once).

There are no tests or lint setup yet.

## Conventions

- Source files use a `PB` prefix (`PBSubsystem.h`, `SPBDashboard.cpp`); class names keep the full `ProtoBoy` prefix. `ProtoBoy.Build.cs` must match the module name and is not renamed.
- **Code goes in its feature's folder**, in `Public/` and `Private/` alike: `Docs/`, `Tasks/`, `Agents/`, `MCP/`, `Sourcing/`, `Health/`, `Net/`, `Daemon/`. A provider or platform gets a subfolder of its feature (`Docs/Obsidian/`, `Tasks/Trello/`, `Sourcing/Fab/`). A feature's MCP tools and prompts live with it (`Docs/PBDocsMCP`, `Tasks/PBTasksMCP`), not in `MCP/`.
- **UI mirrors the feature folders:** each feature's windows, panels and dashboard page go under `UI/<Feature>/` (`UI/Docs/Map/`, `UI/Tasks/Trello/`, ...). Only the dashboard shell, the status bar hub (`PBStatusBarHub`), `PBStyle` and the shared widget builders (`PBWidgets`) sit directly in `UI/`.
- Split a file when it holds more than one job (e.g. the Trello client is one class in three .cpp files by concern). Keep each file's helpers in its own named namespace.
- `UProtoBoySubsystem` owns one service per area; include their headers directly in `PBSubsystem.h` (forward declarations fail with C4150 on the `TUniquePtr` members).
- Team tunables go in `UProtoBoySettings` (saved to the committed `DefaultProtoBoy.ini`); per-user and machine-specific state goes in a JSON store under `Saved/ProtoBoy/`, never in settings.
- UI goes through provider interfaces and services (`IProtoBoyDocsProvider`, `IProtoBoyTaskProvider`, `FProtoBoyTaskBoard`) rather than holding provider state itself.
- Docs are human-readable Markdown that must survive human edits: only append, write new files, or regenerate ProtoBoy's managed blocks. The docs are the model; never add a separate model file.
- Modal windows stall the core ticker: anything that must keep working while one is open (HTTP callbacks, retries, local listeners) is pumped from `UProtoBoySubsystem::OnModalLoopTick`.

## Map

| Area | Main types |
|---|---|
| Core | `FProtoBoyModule` (tab and menu), `UProtoBoySubsystem` (owns the services), `UProtoBoySettings` |
| Docs | `IProtoBoyDocsProvider`, `FProtoBoyDocsService`, `FProtoBoyDocMap` (the docs in layers, from their frontmatter), `FProtoBoyObsidianConnector`, `FProtoBoyDocGenerationRun` |
| Tasks | `IProtoBoyTaskProvider`, `FProtoBoyTrelloClient` / `FProtoBoyTrelloConnector`, `FProtoBoyTaskBoard` (sync state and loaded tasks), `FProtoBoyTaskGenerationRun` |
| Agents | `FProtoBoyAgentRun` (runs an agent's CLI unattended) |
| MCP | `FProtoBoyMCPServer`, `FProtoBoyMCPClientConfig`, tool and prompt registries |
| Sourcing | `ProtoBoyAssetSourcing`, `ProtoBoyFab`, `FProtoBoyAssetSearchRun` (agent-suggested assets) |
| Health | `ProtoBoyDocHealth` (setting, revision control check), `ProtoBoyDocHealthChanges` (files changed since each doc was edited, Git only), `FProtoBoyDocHealthRun` (agent flags stale docs), `ProtoBoyDocHealthReport` (last result), `ProtoBoyDocStructure` (agent-free checks: broken links, missing paths, unwritten sections, coverage) |
| Net | `ProtoBoyHttp` (outgoing HTTP, game-thread callbacks, `RunAfter`) |
| UI | `SProtoBoyDashboard`, `ProtoBoyStatusBarHub` (the status bar button), `SProtoBoyDocMapEditor`, `SProtoBoyTasksPage`, the feature windows |
