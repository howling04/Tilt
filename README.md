# Tilt

The Wandering Luna Unreal Engine 5.8 project, including C++ gameplay, scene assets, source art, and an IoT controller integration interface.

## Getting started

1. Install Git LFS before cloning, then run `git lfs pull` in the clone.
2. Install Unreal Engine 5.8 and a compatible Visual Studio C++ toolchain.
3. Install the plugins enabled in `Luna_v1.uproject`: ModelingToolsEditorMode, Terminal, ModelContextProtocol, EditorToolset, and ProceduralMeshComponent.
4. Generate Visual Studio project files from `Luna_v1.uproject` and build the editor target, or open the project and allow Unreal to build the module.
5. Open `/Game/LunaArt/Maps/L_WanderingLuna`.

The helper scripts in `Scripts/` include machine-specific Unreal Engine installation paths. Adjust those paths before using `Open-LunaEditor.cmd` or `Play-Luna.cmd` on another machine.

## Project layout

- `Source/`: LunaMVP C++ module and Unreal build targets.
- `Config/`: project configuration.
- `Content/`: Unreal scenes, meshes, textures, fonts, and audio.
- `SourceArt/`: original artwork assets.
- `Integration/`: UDP controller interface, endpoint test, and controller schema.
- `Scripts/`: build, launch, asset, and verification helpers.

The controller interface describes an Arduino Nano RP2040 Connect / LSM6DSOX to MQTT, Python, UDP, and UE pipeline. See `Integration/controller.json` for the interface and default ports. This repository contains the UE-side project and integration helpers; the complete upstream controller implementation is not included here.

Unreal-generated binaries, caches, intermediate build files, and saved editor state are excluded. Binary project assets are tracked with Git LFS.
