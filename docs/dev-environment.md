# Development environment

You need a **Windows PC with Dishonored installed** for anything that touches the
game. The apworld and client also run on Linux and macOS.

## Editors: VS Code plus Visual Studio

| Work | Tool | Why |
|---|---|---|
| apworld, client, tools (Python) | **VS Code** | Python + Pylance extensions are all you need. |
| Injected DLL (C++, later) | **Visual Studio 2022 Community** | Easiest way to get the 32-bit MSVC toolchain and to debug a DLL running inside `Dishonored.exe`. |

You can start with VS Code alone and keep using it for the Python side. Install
Visual Studio when you start on `native/`. VS Code with CMake Tools and the MSVC
Build Tools can build C++ too, but Visual Studio's debugger works much better
when you attach to the game, break inside a hook and inspect memory.

## Install list

**Day one**
- Git
- Python **3.12** (Archipelago supports 3.11–3.13)
- VS Code with the recommended extensions (open the repo and accept the prompt;
  see `.vscode/extensions.json`)
- Archipelago, both:
  - the **release** (installer), to play and run the Launcher normally
  - a **source checkout**, to run the world's tests and generate seeds from your working copy

**Reverse engineering (step 1 and 2 of the roadmap)**
- [UE Explorer](https://github.com/UE-Explorer/UE-Explorer): opens Dishonored's
  `.upk` packages and decompiles the UnrealScript. Start here; it shows you which
  classes and functions exist.
- [Cheat Engine](https://www.cheatengine.org/): find values in memory (rune
  count, current map, loading state) and pointer paths to them.
- [x64dbg](https://x64dbg.com/): ships **x32dbg** too, which is the one you need,
  since Dishonored is a 32-bit process.
- [Ghidra](https://ghidra-sre.org/): static analysis of `Dishonored.exe`. You need
  it once you hunt for native functions such as `ProcessEvent`.
- Optional: ReClass.NET (map out structs in memory), System Informer (inspect processes).

**Native DLL (step 4 onward)**
- Visual Studio 2022 Community with the **Desktop development with C++** workload
- CMake (bundled with Visual Studio)
- vcpkg for libraries (MinHook for hooking, apclientpp if the DLL ever talks to the
  server itself)

## Setting up the source checkout

```bash
git clone https://github.com/ArchipelagoMW/Archipelago.git
cd Archipelago
python -m venv .venv
# Windows: .venv\Scripts\activate    Linux/macOS: source .venv/bin/activate
python ModuleUpdate.py   # installs Archipelago's requirements
```

Link this repo's world into it, so edits here are live:

```bash
# Windows (admin or Developer Mode), from the Archipelago folder:
mklink /D worlds\dishonored C:\path\to\dishonored-archipelago\worlds\dishonored
# Linux/macOS:
ln -s /path/to/dishonored-archipelago/worlds/dishonored worlds/dishonored
```

Then, from the Archipelago folder:

```bash
python -m pytest worlds/dishonored/test                  # world tests
python Generate.py                                      # seed from Players/*.yaml
python MultiServer.py output/AP_<seed>.zip               # host it
python Launcher.py "Dishonored Client"                   # or pick it in the Launcher
```

You can also clone Archipelago into `Archipelago/` inside this repo (git ignores
it), and point VS Code at that interpreter so imports like `BaseClasses` resolve.

## Tests in this repo that don't need Archipelago

```bash
pip install pytest
python -m pytest tests          # requirement parser, data files, ID table
python tools/sync_ids.py        # after adding items/locations
python tools/build_apworld.py   # writes dist/dishonored.apworld
```

## Game setup for research

- Use one specific game build and write it down (store and version). Memory
  addresses and offsets only hold for that build.
- Enable the developer console (guide linked in the Manual documentation).
- Keep save files at the start of each mission so you can repeat a pickup over and over.
- Cheat Engine and debuggers trip anti-cheat in online games. Dishonored is
  offline-only, so that's not an issue here, but close Steam overlays and similar
  tools if attaching fails.
