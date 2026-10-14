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
3. Start the game. `archipelago_native.log` appears in the same folder.

To uninstall, delete `dinput8.dll`.

## First test

1. Start the game and load a save, so you're in gameplay rather than the menu.
2. Check `archipelago_native.log`. Expected:
   ```
   Archipelago native module loaded
   ProcessEvent at exe+0x...
   engine tables found
   found PlayerController.ConsoleCommand; ready for console commands
   ```
3. Run `python tools/native_console.py ping`. Expect `pong ready`
   (`pong waiting` means commands aren't running yet; see Troubleshooting).
4. Run `python tools/native_console.py "console GiveRunes 1"`. Your rune count
   should go up, and the log shows `console: GiveRunes 1`.
5. Finally, the real thing: launch the Dishonored Client with `--backend native`.
   Received powers are applied automatically.

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
| nothing, no log file | The game didn't load the DLL. Check it's in `Binaries\Win32` and named `dinput8.dll` |
| `ProcessEvent not found` | Your exe differs from the one the byte pattern came from. Find the function (e.g. with dismod or x32dbg) and set `ProcessEventOffset` |
| `engine tables not usable` | `GNamesOffset`/`GObjectsOffset` are wrong for your build |
| `pong waiting` forever in gameplay | `DrainOn` function never fires. Run with `LogCalls=unique`, find a per-frame function on the PlayerController, add it to `DrainOn` |
| a command runs but nothing happens | Same as typing it in the console: check the name, or it's one of the stripped cheats |

## Build

### Visual Studio 2022 (Windows)

```
cmake -S native -B native/build -A Win32
cmake --build native/build --config Release
```

Output: `native/build/Release/dinput8.dll`. Or open the `native` folder in
Visual Studio ("Open a local folder"); it picks up the CMake project. Pick an
x86 configuration.

### MinGW (Linux, or what CI uses)

```
sudo apt install g++-mingw-w64-i686 cmake
cmake -S native -B native/build-mingw -DCMAKE_TOOLCHAIN_FILE="$PWD/native/cmake-mingw-i686.cmake" -DCMAKE_BUILD_TYPE=Release
cmake --build native/build-mingw
```

## Debugging

Attach Visual Studio (Debug > Attach to Process > `Dishonored.exe`) with a
Debug build to break inside the hook. MinGW builds can be debugged with x32dbg.

## Credits

The `ProcessEvent` byte pattern, the engine table offsets and the object layouts
come from [ectrc/dismod](https://github.com/ectrc/dismod) and its generated SDK.
The code here is written independently. Hooking uses
[MinHook](https://github.com/TsudaKageyu/minhook) (BSD-2-Clause).
