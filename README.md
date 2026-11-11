# ProtoBoy

Game-dev prototyping companion: living docs, health checks, asset lifecycle and toolchain integration.
The design notes are in the Obsidian vault (`Code/ProtoBoy`).

## Layout

```
Unreal/ProtoBoy/        Unreal Engine 5.9 editor plugin
  Source/ProtoBoy/
    ProtoBoyModule        Module startup, Tools menu entry, dashboard tab
    ProtoBoySettings      Project Settings > Plugins > ProtoBoy
    ProtoBoySubsystem     Editor subsystem that owns the services below
    Docs/                 Pillar 1: TDD stubs, ADRs (stub)
    Assets/               Pillar 3: Prototype/Production lifecycle tagging (stub)
    Health/               Pillar 4: health checks, PVI score (stub)
    Daemon/               Client for the future ProtoBoy daemon (stub)
    UI/                   Dashboard widget (placeholder)
```

Later engines and the daemon get their own top-level folders (`Unity/`, `Godot/`, `Daemon/`).

## Using the plugin

Copy or symlink `Unreal/ProtoBoy` into a UE 5.9 C++ project's `Plugins/` folder, then build the project.
Open the dashboard from **Tools > ProtoBoy Dashboard**.

To build the plugin on its own:

```
"C:\Program Files\Epic Games\UE_5.9\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="%CD%\Unreal\ProtoBoy\ProtoBoy.uplugin" -Package="%TEMP%\ProtoBoyBuild" -TargetPlatforms=Win64
```
