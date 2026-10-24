# Roadmap

The goal is to automate, one piece at a time, what the Manual makes the player do
by hand. Every step leaves something playable behind: anything not automated yet
stays on the Manual's honor system.

## Architecture

```
Archipelago server  <-- websocket -->  Dishonored Client (Python)  <-- GameInterface -->  Dishonored.exe
                                        worlds/dishonored/client/        manual | memory | (dll)
```

- **apworld** (`worlds/dishonored/`): items, locations, regions and logic. It reads
  the same JSON as the Manual, and `data/ids.json` gives everything a fixed ID.
- **Client** (`worlds/dishonored/client/`): built on Archipelago's `CommonClient`.
  It only talks to the game through `GameInterface`, so the backend can change
  without touching the rest.
- **Game side**: today, nothing (the `manual` backend). Next, `memory` (reading
  the game from outside with pymem). Possibly later, an injected DLL (`native/`)
  that the client talks to over a named pipe.

## Milestones

### 0. Foundation (done)
- [x] Port the Manual's data to a real apworld with fixed IDs
- [x] Client with `/check` and `/goal`, which plays the same as the Manual
- [x] Tests and CI

### 0.5 In-game DLL, first slice
- [x] `native/`: `dinput8.dll` proxy, `ProcessEvent` hook, named pipe to the client
- [x] Client `--backend native` runs `AddPower` etc. for received items
- [x] Call logging (`LogCalls=unique|filter`, F8 marker) for pickup research
- [x] First in-game test: hook, engine tables, gameplay detection, console commands work
- [x] Events for checks: pickups, map changes, scripted items, objectives
- [ ] Fill `detect` for each location in locations.json (play and copy events)

### 1. Look inside the game
- [ ] Pick and record the game build you'll support
- [ ] Open the packages in UE Explorer and note the classes behind pickups, runes,
      bone charms, blueprints, paintings, the Outsider powers screen, Piero's
      upgrade shop, map loading and the end-of-mission screen (`research/notes.md`)
- [ ] List the console commands and what each does

### 2. First automatic check: missions
- [ ] Use Cheat Engine to find the current map name and the loading/end-of-mission state
      (existing LiveSplit autosplitters for Dishonored are a good shortcut)
- [ ] Implement `MemoryInterface.poll_checked_locations` for the 9 mission locations
- [ ] Implement `poll_goal_reached`

### 3. Pickups
- [ ] Find a way to tell *which* rune, bone charm, painting or blueprint was taken.
      Counting isn't enough, so this probably needs a hook (see `native/`)
- [ ] Fill in `game_object` for every row of `research/locations.csv`

### 4. Items
- [x] Powers: granted automatically with confirmed names, re-applied after loads
- [ ] Powers: stop buying them with runes before they're received (written: `limit`
      requests + `PowerBuy` blocking; untested in-game)
- [ ] Upgrades (Piero's shop). The console cheats are stripped from the release build,
      so the DLL calls the player pawn's `OnGiveUpgrade` with a `UDisSeqAct_GiveUpgrade`
      (`upgrade <name>` request). Names found with `dump` and mapped in
      `client/item_commands.py`, including the second tier of Sword, Sleep Darts,
      Bullets and Grenades. Granting confirmed in-game (Pistol Accuracy 1)
- [ ] Bone charm slots: mapped onto the 2 Piero + 4 DLC statue capacity upgrades
      (`BONE_CHARM_CAPACITY`; a statue grant added a 7th slot in-game). Still to do: cap slots below the base 3 for the
      first copies (DLL), and block buying the Piero ones before they're received
- [ ] Equipment and ammo: the inventory keeps (count, capacity) per ammo type
      (`UDishonoredInventory.m_AmmoInfo`), which could block ammo types until received.
      `dump` shows the current values
- [ ] Bone charm slots
- [ ] Zone keys (hardest; may stay honor-system for a long time)
- [x] Survive save reloads: the client re-applies every received item after each
      load (`gameplay` event). Requires every item's command to set absolute
      state (verify that `AddPower X 1` doesn't downgrade/duplicate when replayed)
- [ ] Block vanilla grants of randomized items. NPCs and level scripts hand out
      powers, equipment and upgrades through Kismet actions that reach the player
      as `OnAddPower`, `OnGiveUpgrade` and `OnGivePickup` (see research/notes.md).
      The DLL sees each one in `ProcessEvent` and can skip it (not call the
      original) when that item is randomized and hasn't been received, and report
      it as an event (it may itself become a check). Same idea for buying powers
      with runes (the power menu calls `DisGFxMoviePlayerHUD.PowerBuy(PowerId, Level)`)
      and upgrades at Piero's. In-game door keys (`OnAddKey`) are not
      randomized and stay untouched; the Archipelago "zone keys" are separate
- Decisions: upgrades are granted outright (no shop changes for now); zone keys
  warn first, block later

- [ ] Show the mana bar when a power is received before the Void (the HUD hides it
      until the Outsider gives the mark). Level scripts toggle it with
      `DisSeqAct_ToggleHUDElement` (`m_HudElement` = `DHE_Mana`); find how that action
      takes effect and do the same from the DLL
- [ ] Option for players without the DLC: 6 Bone Charm Slot items instead of 10 (the
      game caps them at 6 slots; the statue upgrades do nothing)
- [ ] Block the DLC statues (`Twk_Upgrade_Statue*`; removed by the limits, untested): the game hands all four out at the
      pub on the first night, and each adds a bone charm slot

### 5. Saves always match what was received
- [x] Items received after a save come back when that save is loaded: the client
      re-applies everything after each load (confirmed for powers; upgrades are
      re-granted unless already owned)
- [ ] Items *not* received are taken away when a save has them (written for powers and
      upgrades, untested: does removing an upgrade from `m_Upgrades` undo its effect?
      Known gap: loading a save *inside* the Void removes Blink there) (an older seed's
      save, or something bought/scripted before blocking existed). The client sends
      the full received set after each load and the DLL removes the rest: powers
      through the pawn's native `OnRemovePower` handler (the counterpart of
      `OnAddPower`); upgrades by removing them from `m_Upgrades` (check that the
      effect, e.g. bone charm capacity, really goes away)
- [ ] Optional: remember the seed in the save (or next to it) and warn when a
      save from another seed is loaded

### 6. Zone keys
Today they're only shown as received, nothing in the game checks them.
- [ ] Map each zone to its streaming levels (`mapchange L_...` events, research/notes.md)
- [ ] Warn in-game when entering a zone without its key
- [ ] Then block: undo the transition or move the player back. Loading a save
      emits no `mapchange`, so the DLL also needs the list of loaded streaming
      levels (`WorldInfo.StreamingLevels`)

### 7. In-game messages (towards an in-game client)
- [ ] Show "Received X from Y" / "Sent X to Y" on screen. First try the game's own
      `DisSeqAct_ShowHUDMessage` (`m_Message` FString), used by level scripts for HUD
      messages, with the same borrowed-default-object trick as upgrades. Otherwise draw text
      from the DLL when the HUD renders (`HUD.PostRender` passes through
      `ProcessEvent`; the `Canvas` draw functions are callable the same way).
      Fallbacks: call into the Scaleform HUD, or hook Direct3D 9 and draw an overlay
- [ ] Later, possibly: let the DLL connect to the Archipelago server itself
      (websocket + JSON in C++) so the external client becomes optional. The
      Python client stays the reference until then

### Later
- Knife of Dunwall / Brigmore Witches, Dunwall City Trials, "smooth intro"
  (these are mostly data plus options in the apworld)
- DeathLink (already an option; the client needs to detect and cause deaths)

## Possible issues in the Manual data

These are things to check, not changes I made:

- The `Southside Gate` region requires the HoP Distillery, Galvani and Art Dealer
  keys, but not `Southside Gate Zone Key`. So that key is marked progression but
  nothing needs it.
- `Kennels Zone Key` exists as an item but has no matching region.
- The goal location is the only one marked `"victory": true`. Mission victories
  such as `High Overseer Campbell: Victory` are normal checks.
