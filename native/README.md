# Native module (in-game DLL)

A 32-bit DLL that the game loads as `dinput8.dll`. It forwards that DLL's one
function to the real system `dinput8.dll` and, inside the game:

- hooks `UObject::ProcessEvent`, the engine function every script call goes through;
- **grants received items** sent by the Python client on the game thread (`AddPower`
  console commands, upgrades through the pawn's own handler, bone charm capacity);
- **blocks and removes what hasn't been received** (`limit` lines from the client):
  shop and rune purchases, script grants, items in old saves, unreceived ammo and the sword;
- **reports game events** (pickups, objectives, travel prompts, the current area) that
  the client turns into location checks, and refuses travel prompts into keyless zones;
- **draws messages** on the HUD (items sent and received, chat, a main menu notice);
- optionally **logs game function calls**, the research tool for finding
  what fires when you pick up a rune.

The Python client talks to it over the named pipe `\\.\pipe\dishonored_archipelago`
(protocol in `src/pipe_server.hpp`). That is the client's default mode.

**Status:** everything below is confirmed in-game on the Steam build (a full seed played
to its goal, 2026-10-29) unless it says otherwise.

## Install

1. Get `dinput8.dll`: download the `native-dll` artifact from the latest CI run,
   or build it (below).
2. Copy `dinput8.dll` and `archipelago_native.ini` into the game's
   `Binaries\Win32` folder (next to `Dishonored.exe`).
3. Start the game. `archipelago_native.log` appears in the same folder. If that
   folder isn't writable (common under `Program Files`), it goes to
   `%LOCALAPPDATA%\DishonoredArchipelago\archipelago_native.log` instead.

To uninstall, delete `dinput8.dll`.

### With Dishonored Mod Loader

[Dishonored Mod Loader](https://www.nexusmods.com/dishonored/mods/383) (Crimsab) brings its own
`dinput8.dll`, which loads every DLL directly inside `Binaries\Win32\Mods\` and
`Binaries\Win32\NativeMods\` (folders set in its `modloader.ini`). Only one `dinput8.dll` can
exist, so with the loader installed:

1. Keep the loader's `dinput8.dll` in `Binaries\Win32` (not ours).
2. Put ours in `Binaries\Win32\NativeMods\` as `archipelago_native.dll` (any name but
   `dinput8.dll`, `DishonoredLoader.dll` or `ModLoader.dll`, which the loader skips), with
   `archipelago_native.ini` next to it. The CI artifact has this layout in its `NativeMods` folder.
3. The log is then `Binaries\Win32\NativeMods\archipelago_native.log`, and its first line says
   where the DLL was loaded from. The loader's `Binaries\Win32\dinput8_proxy.log` should list
   `ModFolders: loaded: archipelago_native.dll` (confirmed in-game with loader v1.3.15).

If both copies end up installed, the second one to load stays idle ("another copy ... is
already running"). If the loader hooked `ProcessEvent` first, the log says "already hooked by
another mod; hooking on top of it" and both hooks run. Archipelago doesn't appear in the
loader's MODS menu (that needs its plugin format in `Mods\plugins\`).

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
5. Finally, the real thing: open **Dishonored Client** from the Archipelago Launcher.
   Received items are applied automatically.

## Game events (for location checks)

The DLL reports these as they happen. They're written to the log as
`event: ...`, the client shows unmatched ones as `Game event (no location): ...`,
and `python tools/native_console.py "event 0"` (1, 2, ...) reads them directly.

| Event | When |
|---|---|
| `gameplay` | Gameplay starts or the player's state may have been reset: after loading a save, starting a new game, or any level transition (`mapchange`; the Void swaps the inventory out and back). Commands wait 3 s after it. Every level streams into one persistent map, `DishonoredGameFull_P`, so this carries no map name |
| `mapchange <level>` | The game starts loading another level |
| `area <levels>` | Once per load or transition, when gameplay has settled (3 s): the visible area levels (`L_Ovrsr_P`; comma-separated, sublevels left out), i.e. where the player is. Also after a save load, which emits no mapchange. The client sends the player to the main menu (`quittomenu`) when that zone's key is missing |
| `pickup <object path> <class> <item> at=x,y,z` | A pickup actor was taken (destroyed). `<item>` is e.g. `Rune_AbsItm`, `Coins_AbsItm` or `WhaleBoneCharm_01_twk`. Ends in `(load)` when it happened while a level was loading, which is how already-collected pickups are removed from a save, or in `(far)` when it was destroyed more than 1000 units from the player (a mission script removing unused pickups; never a check). Placed pickups have stable paths in their level's script package (`L_Streets1_Script....DisAbstractItemPickup_19`); pickups spawned at runtime sit in `DishonoredGameFull_P` with spawn-order numbers, so match those by position |
| `additem <kismet action path> <item>` | A level script gave the player an item (scripted rewards) |
| `objective <kismet action path> <class> <objectives>` | A level script changed objectives |
| `grant power <name> <level> by <action>` / `grant upgrade <name> by ...` / `grant pickup <tweak> by ...` / `grant inventory <tweak> by ...` | A level script gave the player a power, upgrade, pickup or equipment (`OnAddPower`, `OnGiveUpgrade`, `OnGivePickup`, `OnAddInventoryItem` on the player pawn). E.g. `grant power Blink 1` when the Outsider gives the mark. Our own grants don't produce these |
| `gain item <name> <old>-><new> at=x,y,z` | The player's count of an abstract item went up, whatever the cause: `Rune_AbsItm`, `Coins_AbsItm`, `BoneCharms`, blueprints (`BP_*_AbsItm`), notes, `RewireTool`. Checked every 0.5 s during gameplay; the position is the player's |
| `lose item <name> <old>-><new>` | ...went down (e.g. runes spent on a power) |
| `buy <item\|upgrade\|ammo> <name> for <coins> at=x,y,z` | Coins went down and this went up in the same reading: a shop purchase. E.g. `buy item Rune_AbsItm for 500` (Piero's rune) |
| `gain ammo <type> <old>-><new> at=x,y,z` / `lose ammo ...` | Ammo (`Bullet`, `Arrow`, `Arrow_Sleep`, ...) or elixirs (`HealthElixir`, `ManaElixir`) changed. Firing, drinking and picking up all show up |
| `gain upgrade <name> 0->1 at=x,y,z` / `lose upgrade ...` | An upgrade was added (Piero, a script, or our own `upgrade` request) or removed |
| `choice <i> "<text>" title "<title>" travel=<0\|1> by <node>` | The player confirmed a choice: a scripted choice (travel prompts, the wine, the still; `DisSeqAct_DialogScriptedChoice`) or dialogue. `travel=1`: that option changes the map (confirmed: "Go to Bottle Street") |
| `travel <action path>#<index> "<text>" from <areas>` | A travel option was picked (`<areas>` as in `area`). The destination isn't in the prompt (it's set in the next level's script, not loaded yet), so the client maps each option to the level it loads (`client/zones.py` `TRAVEL_PROMPTS`, from the `mapchange` that follows). Galvani's offices use one level script in two missions, so its prompts are listed per area: `<area>\|<option>` |
| `blocked travel <action path>#<index> from <areas>` | A travel option was refused (`limit travel <option\|<area>\|<option>\|all> 0`): the prompt's stay option is picked instead, so it closes as if declined (confirmed in-game). An option counts as travel if the game flags it as a map change *or* the client sent a `limit travel` line for it (one Golden Cat door changes the map without the flag) |

To turn an event into a check, add a pattern to the location's `detect` list
in `worlds/dishonored/data/locations.json`. A pattern matches when **every word
in it appears in the event** (words compare exactly). `near=x,y,z` matches an
event's `at=` within about 1.5 m, and `!word` requires that word to be absent.
Events tagged `(far)` never match.

```json
"detect": ["pickup L_Streets1_Script.TheWorld.PersistentLevel.DisAbstractItemPickup_19"]
"detect": ["pickup Rune_AbsItm near=1200,-340,95"]
"detect": ["pickup L_Boyle_Int_Script.TheWorld.PersistentLevel.DisAbstractItemPickup_68 !(load)"]
"detect": ["grant pickup Rune_twk by L_Pub_FromBoyle_Script.TheWorld.PersistentLevel.Main_Sequence.DisSeqAct_GivePickup_0"]
"detect": ["objective DisSeqAct_CompleteObjective Flooded_Gear"]
"detect": ["additem ChapterLog_Hub_03"]
```

The victory location's `detect` (`mapchange L_Out_P`, the ending) sends the goal.
`python tools/check_log.py "<mission>"` replays a log against a mission's patterns.

## Blocking and removing what wasn't received

The client sends `limit power <Name> <max level>` and `limit upgrade <name> <0|1>` for
every randomized power and upgrade (after each load or transition, and when an item
arrives). From then on the DLL:

- refuses buying a power with runes beyond its limit (`PowerBuy`): `blocked buy power ...`
- refuses level-script grants beyond the limits (`OnAddPower`, `OnGiveUpgrade`):
  `blocked grant ...`. Except in the Void, which needs Blink to cross; it's removed
  after leaving unless received
- every half second removes powers above their limit (the pawn's `OnRemovePower`,
  then `AddPower` back to the limit) and upgrades not received (taken out of the
  player's upgrade list): `remove power ...` / `remove upgrade ...`
- refuses buying a not-yet-received upgrade at a shop (`Req_BuyItem`; the entry is
  looked up in the shop's displayed list): `blocked buy upgrade ...`. The log line
  `shop: buying entry <n> = <item>` shows what each purchase was (confirmed in-game)
- leaves the DLC statues (`DisUpgrade` actors at the pub) alone. Earlier builds swapped
  their upgrade for Corvo's Mask and hid them; the game's DLC gift code tracks statues by
  upgrade name, and saves made after that crashed on load or on waking up (2026-10-26).
  Statues swapped that way (stored in saves) get their own upgrade back and are shown
  again (`restored world upgrade ...`). The slot a statue adds is cancelled through the
  base capacity (next point)
- keeps equipment not received at 0 (`limit ammo <type> <0|1>`, `limit item <name> <0|1>`):
  ammo of that type (`remove ammo ...`), a pistol/crossbow magazine loaded with it
  (`unload ...`) and collectables like the Rewire Tool (`remove item ...`); shop
  purchases of them are refunded (confirmed in-game)
- takes a sword away until Progressive Sword arrives (`limit equip Sword`): empty hands
  are equipped in its place (`OnEquipItemType`, "Apply" input marked; `unequip sword`),
  then its inventory slot is cleared (`take sword ...`). Once received, Corvo's sword is
  given back (`OnAddInventoryItem`; `give sword ...`) if the sewer gear cutscene has
  happened and the gear isn't in the game's backup. Not in the prologue, whose scripted
  fight hands you one (confirmed in-game: unequip, take, and give back after /send)
- sets the base bone charm capacity (`DisTweaks_PlayerPawn.m_MaxActivatedBoneCharmCount`,
  3 in the shipped game) to `limit bonecharms slots <n>` (items received) minus the
  `Attribute_MaxActivatedBoneCharmCountBonus` of every capacity upgrade owned, every 2 s
  (`bone charm base ... (n received, +b owned)`). So picking up a DLC statue or loading a
  save with upgrades not received doesn't add slots. Capacity upgrades are never taken off
  the upgrade list (that never removed their effect). Confirmed in-game, including a base
  below 0 (-2 with four statues and 2 slots received)
- if a purchase still goes through, refunds the coins: `refund ...`

Confirmed in-game: shop blocking, statue disarming, base capacity, removal from the
upgrade list, refunds, lowering a power, refusing rune purchases (`blocked buy power`),
and holding unreceived ammo/Rewire Tools at 0.
**But removing an upgrade doesn't undo its effect** (bone charm capacity stayed, even
after a reload): the effect is an attribute modifier on the pawn, stored separately.
`removemod <attribute> <modifier>` calls the pawn's `OnRemoveAttributeModifier` to find
the modifier name upgrades use; `dump` lists each upgrade's `attributes=`.

Without limits (no client connected) nothing is blocked or removed.

## On-screen messages

`message <text>` shows a line at the bottom left for 7 seconds (up to 6 at once, fading
out), drawn with the HUD's Canvas right after the game's own `HUD.PostRender` (confirmed
in-game). `{RRGGBB}` inside the text switches colour (default white); the client colours
items, players and locations like Archipelago's text client. Nothing is drawn while the
game is paused (menus, journal) or in the main menu. The client shows every item received
(from `ReceivedItems`, so cheats too), every item found here for someone else, and
Archipelago chat ("Alice: gg", server console as "Server"). Try it:

```
python tools/native_console.py "message Hello from Archipelago"
```

`notice <text>` sets one line drawn centered at the top of the **main menu** only, for up to
60 s, cleared when a game is loaded. The client uses it to say why the player was sent back
there (a missing zone key; confirmed in-game). The log says `messages: drawing the main
menu notice` the first time it's drawn.

`quittomenu` sends the player to the main menu through the pause menu's quit confirmation.

The game's own `DisSeqAct_ShowHUDMessage` can't be used: it has no script handler.

## Upgrades and the state dump (confirmed in-game)

- `upgrade <name>` gives one of Piero's upgrades by its `DisTweaks_Upgrade` object
  name, through the pawn's `OnGiveUpgrade` handler (what level scripts use). It is
  skipped if the player already has it, so replaying after a load is safe. The log
  says `upgrade <name>: granted`, `already owned`, or why it failed.
- `dump` writes to the log: every upgrade the game has loaded (object name, in-game
  name, the item it belongs to, and `OWNED`), the bone charm slots, the inventory
  slots, ammo count/capacity per type, and abstract items (runes, blueprints, ...).

```
python tools/native_console.py dump
python tools/native_console.py "upgrade <name from the dump>"
```

Both run once you're in gameplay as Corvo (they wait while you're possessing something).

## Researching pickups (call logging)

In `archipelago_native.ini`, set `[Research] LogCalls=`:

- `unique`: logs each function the first time it runs. Cheap, and good for
  discovering names: walk to a rune, press **F8** (writes a marker), pick it up,
  and every *new* function after the marker is a candidate.
- `filter`: logs every call whose function or object name contains a word in
  `LogFilter` (default `Pickup,Rune,Charm,Upgrade`), with the exact object name
  of the pickup. That object name is what `research/locations.csv` needs.
  This is slow; expect low frame rates while it's on.

**F8** also works during normal play: it writes `==== MARK (F8) at=x,y,z chaos=N ====` with
the player's position and chaos score, to flag a check that produced no event. **F9** pauses/resumes call logging.

## Troubleshooting

| Log says | Meaning / fix |
|---|---|
| no log file in either place | The game didn't load the DLL. Check it's in `Binaries\Win32`, named exactly `dinput8.dll` (Windows may hide the extension, so watch for `dinput8.dll.dll`) |
| `ProcessEvent not found` | Your exe differs from the one the byte pattern came from. Find the function (e.g. with dismod or x32dbg) and set `ProcessEventOffset` |
| `engine tables not usable` | `GNamesOffset`/`GObjectsOffset` are wrong for your build |
| `pong waiting` forever in gameplay, no `in gameplay` line | The DLL can't see the player's pawn. Send the log; the offsets in `src/ue3.hpp` may differ for your build |
| `EnableCheats did not create a CheatManager` | The DLL couldn't switch cheats on (it retries every 5s). Items wait meanwhile, and `ping` answers `pong nocheats`. Enable the console in-game the usual way (F1) and they'll go through. Please send the log line, including the `CheatClass` it names |
| Steam: "cloud status out of sync" at every launch | Steam Cloud doesn't sync our files (only saves), but it can't sync after an unclean exit. Quit through the menu, not Alt+F4/Task Manager. The log should end with `game is exiting; going idle`. If the prompt stays even after clean exits, rename `dinput8.dll` for two launches to see whether the mod is involved, and report it |
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

The MinGW build is checked by CI. The MSVC toolchain (32-bit, with CMake and Ninja) has
built the locally installed DLLs since 2026-10-19 and works. If it fails for you, paste the first
error; meanwhile, use the CI download.

## Debugging

Attach Visual Studio (Debug > Attach to Process > `Dishonored.exe`) with a
Debug build to break inside the hook. MinGW builds can be debugged with x32dbg.

## Credits

The `ProcessEvent` byte pattern, the engine table offsets and the object layouts
come from [ectrc/dismod](https://github.com/ectrc/dismod) and its generated SDK.
The code here is written independently. Hooking uses
[MinHook](https://github.com/TsudaKageyu/minhook) (BSD-2-Clause).
