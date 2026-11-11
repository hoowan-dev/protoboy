# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

ProtoBoy is a game-dev prototyping companion: living docs, asset lifecycle tagging, project health checks, and toolchain integration. The full concept and roadmap are in the Obsidian vault at `C:\Users\becer\Documents\Obsidian\Hoowan Vault\Code\ProtoBoy\ProtoBoy Ideas.md`. Read it before adding features: the code is organized by the "pillars" defined there.

The repo currently contains only the Unreal Engine 5.9 editor plugin (`Unreal/ProtoBoy`). It is intentionally a base: most functions are stubs marked `// TODO` that describe the intended behavior. Keep new work simple and stub-first unless asked to implement something fully. Future engines and the daemon go in sibling top-level folders (`Unity/`, `Godot/`, `Daemon/`).

## Build

UE 5.9 is installed at `C:\Program Files\Epic Games\UE_5.9`. There is no host project in the repo; build the plugin on its own with BuildPlugin. Package to a temp/scratch folder, never inside the repo:

```
"C:\Program Files\Epic Games\UE_5.9\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="C:\dev\protoboy\Unreal\ProtoBoy\ProtoBoy.uplugin" -Package="<temp dir>" -TargetPlatforms=Win64
```

- Do **not** pass `-NoHostPlatform`. It skips the editor target, so the editor-only module never compiles and the build "succeeds" without checking anything.
- Success means `BUILD SUCCESSFUL` and `UnrealEditor-ProtoBoy.dll` in `<temp dir>/Binaries/Win64/`. When filtering output, grep for `error|warning C|BUILD SUCCESSFUL|ExitCode`.
- Check engine APIs against the headers under `Engine/Source` rather than from memory, since 5.9 may differ from older versions.

There are no tests or lint setup yet.

## Architecture

Single module `ProtoBoy` (Type `Editor`, `PostEngineInit`).

- `FProtoBoyModule` handles only editor UI registration: the `ProtoBoyDashboard` nomad tab and the **Tools > ProtoBoy Dashboard** menu entry (via `UToolMenus` startup callback, owner-scoped so shutdown unregisters it).
- `UProtoBoySubsystem` (a `UEditorSubsystem`) is the central hub. It owns one plain C++ service per pillar as `TUniquePtr` and is where editor events (asset registry, package save, live coding) should be bound and routed (`BindEditorEvents`, currently a TODO). Access it with `UProtoBoySubsystem::Get()`.
  - `FProtoBoyDocGenerator` (Docs/): pillar 1, Markdown TDD stubs and ADRs written under the configured docs folder.
  - `FProtoBoyAssetLifecycle` (Assets/): pillar 3, Untracked/Prototype/Production tags and the Greybox Replacement Matrix.
  - `FProtoBoyHealthChecker` (Health/): pillar 4, health issues and the Prototype-to-Production Viability Index.
  - `FProtoBoyDaemonClient` (Daemon/): link to the planned engine-agnostic daemon over local WebSocket.
- `UProtoBoySettings` (`UDeveloperSettings`, `Config=ProtoBoy`, `DefaultConfig`) holds all tunables and saves to the host project's `DefaultProtoBoy.ini`. Add new options here rather than hard-coding them.
- The service headers are included directly in `ProtoBoySubsystem.h`, not forward-declared. UHT-generated code destroys the `TUniquePtr` members, and forward declarations there fail with C4150 (incomplete type).
- Design intent from the concept doc: docs are human-readable Markdown that must survive human edits (regenerate only auto-generated blocks), and heavy analysis is meant to move to the daemon eventually so the editor stays responsive.
