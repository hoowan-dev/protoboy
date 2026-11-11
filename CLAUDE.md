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

Source files use a `PB` prefix (`PBSubsystem.h`, `SPBDashboard.cpp`); class names keep the full `ProtoBoy` prefix. `ProtoBoy.Build.cs` must match the module name and is not renamed.

Single module `ProtoBoy` (Type `Editor`, `PostEngineInit`).

- `FProtoBoyModule` handles only editor UI registration: the `ProtoBoyDashboard` nomad tab and the **Tools > ProtoBoy Dashboard** menu entry (via `UToolMenus` startup callback, owner-scoped so shutdown unregisters it).
- `UProtoBoySubsystem` (a `UEditorSubsystem`) is the central hub. It owns one plain C++ service per pillar as `TUniquePtr` and is where editor events (asset registry, package save, live coding) should be bound and routed (`BindEditorEvents`, currently a TODO). Access it with `UProtoBoySubsystem::Get()`.
  - `FProtoBoyDocGenerator` (Docs/): pillar 1, Markdown TDD stubs and ADRs written under the docs root (the Obsidian docs folder when connected, otherwise `DocsDirectory`).
  - `FProtoBoyObsidianConnector` (Docs/Obsidian/): file-system link to an Obsidian vault (a folder with `.obsidian/`). Finds vaults via `%APPDATA%/obsidian/obsidian.json`, validates, creates the docs folder and a `_ProtoBoy.md` marker note, and saves the connection through `FProtoBoyDocsConnectionStore`. UI lives in `SProtoBoyObsidianPanel` (UI/Obsidian/). Platform-specific code goes in a per-platform subfolder (`Docs/<Platform>/`, `UI/<Platform>/`); shared docs code stays in `Docs/`.
  - `FProtoBoyAssetLifecycle` (Assets/): pillar 3, Untracked/Prototype/Production tags and the Greybox Replacement Matrix.
  - `FProtoBoyHealthChecker` (Health/): pillar 4, health issues and the Prototype-to-Production Viability Index.
  - `FProtoBoyDaemonClient` (Daemon/): link to the planned engine-agnostic daemon over local WebSocket.
  - `FProtoBoyMCPServer` (MCP/): ProtoBoy as an MCP server on `127.0.0.1` that agents (Claude Code, Claude Desktop, Gemini CLI) connect to and call ProtoBoy tools; ProtoBoy holds no LLM API keys. Streamable HTTP via the engine's `HTTPServer` module (handlers run on the game thread). Dual-era: modern stateless requests (2026-07-28, per-request `_meta`, mirrored `Mcp-*` headers validated) and legacy `initialize`-handshake clients (2025-03-26..2025-11-25) on the same `/mcp` endpoint. Modal windows stall the core ticker, so the MCP window calls `TickWhileModal`. Owns the tool list and the per-user bearer token (saved by `FProtoBoyMCPConnectionStore` to `Saved/ProtoBoy/MCPConnection.json`). `FProtoBoyMCPClientConfig` adds/removes ProtoBoy's entry in each client's config (JSON merge with a `.protoboy.bak` backup; Claude Code via the `claude mcp` CLI) and builds the manual-setup snippet; `FProtoBoyMCPServer::ConnectClient` is the one-click entry point. Team-wide options (port, write-tool gating, disabled tools) live in `UProtoBoySettings`. UI is `SProtoBoyConnectLLMDialog` with panels in UI/MCP/. Intended to move into the daemon later.
- `UProtoBoySettings` (`UDeveloperSettings`, `Config=ProtoBoy`, `DefaultConfig`) holds all tunables and saves to the host project's `DefaultProtoBoy.ini`. Add new options here rather than hard-coding them. Machine-specific state (absolute paths, per-user connections) does not go here, since that ini is committed.
- `FProtoBoyDocsConnectionStore` (Docs/PBDocsConnection.h) serializes the user's documentation connection (`FProtoBoyDocsConnection` USTRUCT, via `FJsonObjectConverter`) to `<Project>/Saved/ProtoBoy/DocsConnection.json`. Add fields to the struct; bump `CurrentVersion` only for breaking changes. `FProtoBoyObsidianConnector::OnConnectionChanged` fires after Connect/Disconnect so UI can refresh.
- The service headers are included directly in `PBSubsystem.h`, not forward-declared. UHT-generated code destroys the `TUniquePtr` members, and forward declarations there fail with C4150 (incomplete type).
- Design intent from the concept doc: docs are human-readable Markdown that must survive human edits (regenerate only auto-generated blocks), and heavy analysis is meant to move to the daemon eventually so the editor stays responsive.
