# Native module (in-game DLL)

A 32-bit DLL that the game loads as `dinput8.dll`. It forwards that DLL's one
function to the real system `dinput8.dll` and, inside the game:

- hooks `UObject::ProcessEvent`, the engine function every script call goes through;
- runs **console commands sent by the Python client** on the game thread
  (`AddPower Blink 1`, ...), so received items are applied without typing;
- optionally **logs game function calls**, the research tool for finding
  what fires when you pick up a rune.

The Python client talks to it over the named pipe `\\.\pipe\dishonored_archipelago`
(protocol in `src/pipe_server.hpp`), using `--backend native`.

**Status:** compiles (MinGW cross-build, checked in CI) but **has not been run
in the game yet**. Expect the first in-game test to need fixes; the log file
says where it stopped.

## Install

1. Get `dinput8.dll`: download the `native-dll` artifact from the latest CI run,
   or build it (below).
2. Copy `dinput8.dll` and `archipelago_native.ini` into the game's
   `Binaries\Win32` folder (next to `Dishonored.exe`).
3. Start the game. `archipelago_native.log` appears in the same folder. If that
   folder isn't writable (common under `Program Files`), it goes to
   `%LOCALAPPDATA%\DishonoredArchipelago\archipelago_native.log` instead.

To uninstall, delete `dinput8.dll`.

## First test

1. Start the game and load a save, so you're in gameplay rather than the menu.
2. Check `archipelago_native.log`. Expected:
   ```
   Archipelago native module loaded
   ProcessEvent at exe+0x...
   engine tables found
   found DishonoredEngine (...) and PlayerController.ConsoleCommand
   in gameplay
   ```
3. Run `python tools/native_console.py ping`. Expect `pong ready`
   (`pong waiting` means commands aren't running yet; see Troubleshooting).
4. Run `python tools/native_console.py "console GiveRunes 1"`. Your rune count
   should go up, and the log shows `console: GiveRunes 1`.
5. Finally, the real thing: launch the Dishonored Client with `--backend native`.
   Received powers are applied automatically.

## Game events (for location checks)

The DLL reports these as they happen. They're written to the log as
`event: ...`, the client shows unmatched ones as `Game event (no location): ...`,
and `python tools/native_console.py "event 0"` (1, 2, ...) reads them directly.

| Event | When |
|---|---|
| `gameplay` | Gameplay starts (after loading a save or a level transition). Every level streams into one persistent map, `DishonoredGameFull_P`, so this carries no map name |
| `mapchange <level>` | The game starts loading another level |
| `pickup <object path> <class> <item> at=x,y,z` | A pickup actor was taken (destroyed). `<item>` is e.g. `Rune_AbsItm`, `Coins_AbsItm` or `WhaleBoneCharm_01_twk`. Ends in `(load)` when it happened while a level was loading, which is how already-collected pickups are removed from a save. Placed pickups have stable paths in their level's script package (`L_Streets1_Script....DisAbstractItemPickup_19`); pickups spawned at runtime sit in `DishonoredGameFull_P` with spawn-order numbers, so match those by position |
| `additem <kismet action path> <item>` | A level script gave the player an item (scripted rewards) |
| `objective <kismet action path> <class> <objectives>` | A level script changed objectives |

To turn an event into a check, add a pattern to the location's `detect` list
in `worlds/dishonored/data/locations.json`. A pattern matches when **every word
in it appears in the event** (words compare exactly). `near=x,y,z` matches an
event's `at=` within about 1.5 m.

```json
"detect": ["pickup L_Streets1_Script.TheWorld.PersistentLevel.DisAbstractItemPickup_19"]
"detect": ["pickup Rune_AbsItm near=1200,-340,95"]
"detect": ["objective DisSeqAct_CompleteObjective Optional_GrannyRagsHook"]
"detect": ["mapchange L_Ovrsr_P"]
```

Put a `detect` on the victory location to make finishing the game send the goal.

## Researching pickups (call logging)

In `archipelago_native.ini`, set `[Research] LogCalls=`:

- `unique`: logs each function the first time it runs. Cheap, and good for
  discovering names: walk to a rune, press **F8** (writes a marker), pick it up,
  and every *new* function after the marker is a candidate.
- `filter`: logs every call whose function or object name contains a word in
  `LogFilter` (default `Pickup,Rune,Charm,Upgrade`), with the exact object name
  of the pickup. That object name is what `research/locations.csv` needs.
  This is slow; expect low frame rates while it's on.

**F9** pauses/resumes call logging.

## Troubleshooting

| Log says | Meaning / fix |
|---|---|
| no log file in either place | The game didn't load the DLL. Check it's in `Binaries\Win32`, named exactly `dinput8.dll` (Windows may hide the extension, so watch for `dinput8.dll.dll`) |
| `ProcessEvent not found` | Your exe differs from the one the byte pattern came from. Find the function (e.g. with dismod or x32dbg) and set `ProcessEventOffset` |
| `engine tables not usable` | `GNamesOffset`/`GObjectsOffset` are wrong for your build |
| `pong waiting` forever in gameplay, no `in gameplay` line | The DLL can't see the player's pawn. Send the log; the offsets in `src/ue3.hpp` may differ for your build |
| `EnableCheats did not create a CheatManager`, commands "not recognized" | Cheats couldn't be switched on from the DLL. Enable the console in-game the usual way (your console unlocker's key), then retry |
| a command runs but nothing happens | Same as typing it in the console: check the name, or it's one of the stripped cheats |

## Build

You don't have to build it to try it: every push to GitHub builds `dinput8.dll`
in CI. Open the repository's **Actions** tab, click the latest **CI** run, and
download **native-dll** under *Artifacts* (you must be signed in to GitHub). It's a
zip containing `dinput8.dll` and `archipelago_native.ini`.

Build it yourself when you want to change the C++.

### Windows, Visual Studio 2022

One-time setup:

1. Install [Visual Studio 2022 Community](https://visualstudio.microsoft.com/vs/community/).
   In the installer, tick the **Desktop development with C++** workload. Its
   defaults include the MSVC compiler, the Windows SDK and "C++ CMake tools for
   Windows", which is everything this needs.
2. Install [Git for Windows](https://git-scm.com/download/win) if you don't have it.
   The build downloads MinHook with git.

**From the command line** (simplest): open **Developer PowerShell for VS 2022**
from the Start menu, go to the repo, and run:

```powershell
cd native
cmake --preset windows-x86
cmake --build --preset windows-x86-release
```

The DLL is at `native\build\windows-x86\Release\dinput8.dll`.

**From the Visual Studio UI:** *File > Open > Folder...* and pick the `native`
folder. When it asks, choose the **Windows 32-bit (Visual Studio 2022)** preset
(it's also in the toolbar dropdown), then *Build > Build All*. Don't use the
default x64 configuration: the build refuses 64-bit on purpose, because the
game is 32-bit.

The first configure takes a minute because it downloads MinHook.

### Linux (what CI does)

```bash
sudo apt install g++-mingw-w64-i686 cmake
cd native
cmake --preset mingw-x86
cmake --build --preset mingw-x86
```

Output: `native/build/mingw-x86/dinput8.dll`. It runs on Windows like the
Visual Studio build.

### If the Visual Studio build fails

The MinGW build is checked by CI; the Visual Studio build hasn't been tried
yet. Paste the first error and it should be a small fix. Meanwhile, use the CI
download.

## Debugging

Attach Visual Studio (Debug > Attach to Process > `Dishonored.exe`) with a
Debug build to break inside the hook. MinGW builds can be debugged with x32dbg.

## Credits

The `ProcessEvent` byte pattern, the engine table offsets and the object layouts
come from [ectrc/dismod](https://github.com/ectrc/dismod) and its generated SDK.
The code here is written independently. Hooking uses
[MinHook](https://github.com/TsudaKageyu/minhook) (BSD-2-Clause).
