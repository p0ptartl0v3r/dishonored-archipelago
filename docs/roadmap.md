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
- [ ] Equipment and ammo
- [ ] Powers (block buying them, or grant them and hand out runes)
- [ ] Upgrades (Piero's shop). The console cheats are stripped from the release build,
      so call the player pawn's `OnGiveUpgrade` with a `UDisSeqAct_GiveUpgrade` through the DLL
- [ ] Bone charm slots
- [ ] Zone keys (hardest; may stay honor-system for a long time)
- [x] Survive save reloads: the client re-applies every received item after each
      load (`gameplay` event). Requires every item's command to set absolute
      state (verify that `AddPower X 1` doesn't downgrade/duplicate when replayed)
- Decisions: upgrades are granted outright (no shop changes for now); zone keys
  warn first, block later

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
