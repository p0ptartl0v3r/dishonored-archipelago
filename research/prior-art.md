# Prior art: existing Dishonored mods to learn from

Surveyed 2026-10-14. Nothing here has a license unless stated, so **read and
learn from it, but don't copy code into this repo** without the author's
permission. Facts (class names, offsets, byte signatures) are fine to note.

## ectrc/dismod (most useful)

<https://github.com/ectrc/dismod>, a multiplayer experiment, last commit March 2026. No license file.

What it shows:

- **The architecture we planned for `native/`, working:** a 32-bit C++ DLL
  loaded by a `dinput8.dll` proxy (`d8input.def`), built with CMake + MSVC, using
  MinHook for hooks and libhat for finding functions by byte pattern.
- **A generated SDK of every game class** (`external/sdk/`), made with the
  CodeRed generator (CodeRedModding). `DishonoredGame_classes.hpp` is a
  2.5 MB, searchable list of every class, property (with memory offset) and
  function. **Search this instead of clicking through UE Explorer.**
- **A `ProcessEvent` hook with a logging switch** (`LOG_PE` in
  `src/hooks/engine/process_event.h`). With it on, the DLL logs every script
  function call. Pick up a rune and the log shows exactly what fires.
- The offsets it uses (`GObjects` at exe+0x1023630, `GNames` at exe+0x1035674)
  are for the author's game build, which may not be yours. Hooks are found by
  byte pattern instead, which survives small exe changes better.

## What the SDK says about our locations

| Check | Classes / data | Notes |
|---|---|---|
| All pickups | `ADisPickup_Base` (extends `ADishonoredKActor`) | Shared base class. Hook its pickup path to catch everything |
| Runes | probably `ADisAbstractItemPickup` with `UDisTweaks_AbstractItemPickup.m_pAbstractItem` = the rune `UDisAbstractItem` | `UDisTweaks_PlayerPawn.m_pRuneItem` is the rune item. The `*_AbstractInv_MS_SF.upk` packages are likely these definitions. **Verify** |
| Bone charms | `ADisWhaleBoneCharm` (`UDisTweaks_WhaleBoneCharm`, `UDisTweaks_WhaleBoneCharmList`) | DLC cracked charms: `ADisDLC07WhaleBoneCharmCracked` |
| Paintings | stat `ePlayerStat_SokolovPaintingFound` | Pickup class not identified yet |
| Blueprints | probably abstract items (`Blueprints_twk_Abst_SF.upk`) | **Verify** |
| Counters | `EPlayerStat`: `RuneFound` (34), `BoneCharmFound` (35), `OutsiderShrineFound` (36), `SokolovPaintingFound` (37) | Counts only: shows *that* one was found, not *which* |
| Kismet event | `UDisSeqEvent_PickupPickedUp` | Level scripts react to pickups through this; worth seeing in the `LOG_PE` output |
| Mission end | `UDisSeqAct_ShowMissionStats`, `UDisGFxMoviePlayerMissionStats` | Candidate for the mission-complete signal |

## What the SDK says about our items

`UDishonoredCheatManager` has console commands that map almost one-to-one onto
our item types. Because `giverunes` works in the shipped game, try the others
in the console:

| Command | Our items |
|---|---|
| `AddPower <Name> <Level>`, `RemovePower <Name>`, `MinPowers`, `MaxPowers` | Progressive Blink / Dark Vision / ... (power name values unknown yet) |
| `AddUpgrade <Name>`, `RemoveUpgrade <Name>`, `MinUpgrades`, `MaxUpgrades` | Equipment upgrades (names probably in `Twk_Upgrades_SF.upk`) |
| `GiveBoneCharm` | Probably gives a random charm. Slots are a separate problem |
| `GivePlayerItem <TweakString>` | Equipment / ammo / gadgets |
| `GiveRunes <N>`, `GiveMoney <N>` | Runes for buying powers, filler |
| `AbstractInventoryList` | **Run this first**: it should list the abstract item names (rune, blueprints...) |

On the pawn there are matching Kismet handlers (`OnAddPower`, `OnRemovePower`,
`OnGiveUpgrade`, `OnGivePickup`, `OnAddKey`), so the game itself grants these
the same way.

## Other repos

- **deadYokai/dishonored-toolkit** (Python, no license): reads and patches
  `.upk` files once they are decompressed with Gildor's decompressor. Could list
  every placed pickup in a level package from a script. A Rust rewrite is on GitLab
  (`deadYokai/ue3-tools`).
- **ProfessorFrog/DishonoredCuhrayzeeMod**, **josephdviviano/dis-ult-diff-mod**
  (2016): gameplay changes made purely through the `Dishonored*.ini` config files.
  Their ini files show how items, powers and upgrades are configured.
- **MinusGix/DishonoredMods** (2018): binary patches only.

## Nexus

- **Dishonored Mod Loader** by Crimsab (nexusmods.com/dishonored/mods/383): adds a
  MODS entry to the pause menu and builds settings menus from each mod's config
  definition. Mods that use it install into `Binaries/Win32`. No public source
  found. Could later host our connection settings (server, slot), but the core
  shouldn't depend on it.
  Inspected v1.3.15 without running it (2026-10-30; exports, imports, strings): its
  `dinput8.dll` forwards DirectInput to the system DLL, loads `Mods\DishonoredLoader.dll`
  (the framework: MODS pause-menu entry, `Mods\plugins\*.dll` plugins exporting `Plg_Init`,
  `Plg_QueryMenu`, `ModInfo`, and INI mods), then every DLL directly in the folders of
  `modloader.ini` `[Loader] ModFolders=` (default `Mods,NativeMods`), skipping `dinput8.dll`,
  `DishonoredLoader.dll` and `ModLoader.dll`. So our DLL runs from `NativeMods\` under another
  name. It logs to `modloader.log` / `blink_bootstrap.log`. Whether it hooks `ProcessEvent`
  is unknown; ours chains over an existing hook if it does.
- A closed-source co-op mod (installs a `d3d9.dll`) appeared in September 2026,
  which is part of why Nexus looks busy. Don't run closed-source DLLs on a
  machine you care about.
