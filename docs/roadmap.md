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
- [ ] Fill `detect` for each location in locations.json (play and copy events). Done and
      confirmed from play: Intro, High Overseer Campbell, House of Pleasure, The Royal Physician
      (`tools/check_log.py`)

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
- [x] Find a way to tell *which* rune, bone charm, painting or blueprint was taken
      (the DLL's `pickup`/`grant`/`objective`/`additem` events)
- [x] Fill in `game_object` for every row of `research/locations.csv`: all 100 locations have
      detect patterns, each seen in play (2026-10-29, one low-chaos playthrough)
- [ ] High chaos: Kingsparrow and the lighthouse load `_HighChaos_P` levels, and other
      missions have chaos variants (`L_Brothel_HighChaos`): replay those to confirm the
      same names match, or add theirs
- [ ] Choices not taken: e.g. sparing Boyle vs not, other Boyle sister bedrooms (all three
      paintings are listed), Emily's hiding spots (matched by the grant, so any spot)

### 4. Items
- [x] Powers: granted automatically with confirmed names, re-applied after loads
- [x] Powers: stop buying them with runes before they're received (`PowerBuy` is
      refused; confirmed in-game. Its PowerId is lower case: "blink")
- [ ] Upgrades (Piero's shop). The console cheats are stripped from the release build,
      so the DLL calls the player pawn's `OnGiveUpgrade` with a `UDisSeqAct_GiveUpgrade`
      (`upgrade <name>` request). Names found with `dump` and mapped in
      `client/item_commands.py`, including the second tier of Sword, Sleep Darts,
      Bullets and Grenades. Granting confirmed in-game (Pistol Accuracy 1)
- [x] Bone charm slots: base capacity set by the DLL (confirmed: 1 item = 1 slot);
      mapped onto the 2 Piero + 4 DLC statue capacity upgrades
      (`BONE_CHARM_CAPACITY`; a statue grant added a 7th slot in-game). Still to do: cap slots below the base 3 for the
      first copies (DLL), and block buying the Piero ones before they're received
- [ ] Equipment: bullets, bolts, sleep darts, grenades (+ explosive/incendiary/sticky),
      spring razors and the Rewire Tool are held at 0 until received (`limit ammo`,
      `limit item`; confirmed in-game). The sword: unequipped, taken out of the
      inventory, and given back when received (confirmed in-game).
      Optional: refuse ammo purchases up front instead of refunding (shop entries:
      Bolt_Ammo_twk, SleepDart_Ammo_twk, Bullets_Store_Ammo_twk,
      SpringRazor_Ammo_WithItem_twk, Grenade_Ammo_WithItem_twk, RewireTool_twk)
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
- [x] Option for players without the DLC (`void_walkers_arsenal` off): 6 Bone Charm Slot
      items instead of 10 (the game caps them at 6 slots; the statue upgrades do nothing)
- [x] Block the DLC statues (the game hands all four out at the pub on the first night,
      and each adds a bone charm slot). Pointing them at Corvo's Mask worked at first but
      broke saves (crash on load / on waking up: the DLC gift code tracks statues by
      upgrade name), so statues are left alone again and swapped ones restored (2026-10-26,
      confirmed working: they were restored). Their bonus is now cancelled through the
      bone charm base (received slots minus the bonus of every capacity upgrade owned;
      confirmed 2026-10-26: all four statues picked up with 2 slots received kept 2 slots,
      base -2 works). Also covers mission-select snapshots and old saves, as long as the
      upgrade is still on the list

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
- [x] Map each zone to its streaming levels (`client/zones.py`; confirmed for the first
      two missions, the rest from package names where unambiguous)
- [ ] Show received zone keys in the journal's Key Ring tab: give a key through the
      pawn's native `OnAddKey` (`DisSeqAct_AddKey`: `m_Name`, `m_KeyTweaks`) like
      upgrades. Open question: can the entry show a custom name?
- [ ] Warn in-game when entering a zone without its key (written: the client shows
      "You don't have the X Zone Key yet" on mapchange; untested. A loaded save emits
      no mapchange, so loading inside a zone doesn't warn yet)
- [ ] Refuse travel prompts into zones without their key (written): prompts are
      `DisSeqAct_DialogScriptedChoice` actions whose options flag a map change; the DLL
      picks the "stay" option instead (refusal confirmed in-game with `limit travel all 0`).
      Each option is mapped to its destination level in `zones.py` `TRAVEL_PROMPTS` from
      play logs (`travel ...` then `mapchange ...`) and `dump` (lists every travel prompt
      in the loaded levels). High Overseer Campbell: all 19 prompts mapped, refusal confirmed.
      House of Pleasure: 21 prompts mapped (2026-10-26; Galvani's exits keyed per area).
      The Royal Physician: 6 prompts, its four bridge areas mapped to their keys
      for the Kennels. Other missions: run `dump` once per area
- [x] Forced entries (story transitions, save loads, mission select, doors missing from
      the table) into keyless zones: the DLL reports `area <levels>` once settled; the client
      warns, then sends `quittomenu` 6 s later unless they walk out first, and the main menu
      shows why (`notice`). Confirmed in-game for the Kennels
- [x] Option `starting_zone_keys` (none / prison / intro) so the end of the Prologue
      doesn't depend on other players
- [ ] Earlier plan (2026-10-25; replaced by the two items above): refuse player-initiated transition prompts into
      a zone without its key ("Requires the X Zone Key"), and hold back checks found in
      zones the story puts you in without the key (Coldridge, sewers, Void, pub, any
      mission start), sending them once the key arrives. Story transitions (cutscenes,
      mission ends) are never blocked. Research first: what runs when a travel prompt is
      used (call logging around a transition); the move itself is native
      (`DisSeqAct_GotoPlayerTravelDestination`, no handler). Rejected for now: visible
      barriers (hand-placed per transition, Blink gets around them) and pushing the
      player back (fragile around mission ends)
- [ ] Then block: undo the transition or move the player back. Loading a save
      emits no `mapchange`, so the DLL also needs the list of loaded streaming
      levels (`WorldInfo.StreamingLevels`)

- [ ] Option: missions as separate logic (user idea, 2026-10-26). Today the regions are one
      chain of ~30 zone keys (Prologue -> ... -> Lighthouse), so late missions need nearly
      every earlier key. With mission select, each mission's first area could connect from
      the start and need only its own key; the Pub becomes its own area reached after any
      mission's end (a forced story transition). Only sound if every mission can actually be
      started without reaching it in the story: find out which missions the main menu's
      mission select lists before the game is finished, and what gear a mission started from
      it gives (`DisGFxMoviePlayerMainMenu.OnMissionSelected(MissionIdx)` is native). If it
      only lists missions reached, the DLL would have to unlock them.
      Found 2026-10-26: on a profile that has finished the game, every mission is listed
      (unknown for an unfinished profile). A mission started from it restores the loadout
      the player had when they first started that mission (Lady Boyle: no Wind Blast, Bend
      Time or Swarm; The Loyalists: all three, plus more upgrades). The client/DLL already
      correct that to the received items after every load, except bone charm capacity:
      a snapshot (or any old save) owning unreceived capacity upgrades keeps their bonus,
      because removing an upgrade doesn't undo its effect (see research/notes.md, Powers).
      Snapshots made during an Archipelago run are safe (capacity purchases were blocked),
      unless the profile is reused across seeds. Research before building this option:
      (1) read the capacity bonus in effect (the attribute lives in a native map; look for
      a readable effective value on the pawn) and lower the base to match (base 0 has
      worked down to 1 already); or (2) cancel it with the cheat `AddMod
      Attribute_MaxActivatedBoneCharmCountBonus <name> <type> -N`, if it survived the
      release build like `AddPower` did. Decision 2026-10-26: keep the Manual's linear
      logic until then.
      Update 2026-10-26: capacity is solved (the base subtracts the bonus owned; tested from
      mission select: The Loyalists' snapshot with both Piero upgrades ended at the 2 slots
      received, base -1). In the same run every unreceived power, upgrade, ammo type and the
      Rewire Tool were removed within a second. Still open: other removed upgrades may keep
      their effect (e.g. Silent Running), and whether an unfinished profile lists every mission
- [x] Backyard bone charms were swapped (fixed 2026-10-26): Rat Infested Building is the vise,
      which destroys a static Ovrsr_NoPhysBoneCharm and spawns a loose Ovrsr_PhysBoneCharm
      (matched by name and position); Overseer Stolen is DisWhaleBoneCharm_0

### 7. In-game messages (towards an in-game client)
- [x] Show "Received X from Y" / "Sent X to Y" on screen: drawn with the HUD's Canvas
      after `HUD.PostRender` (`message <text>`, colour markers `{RRGGBB}`; drawing
      confirmed in-game, colours/pause/client side untested). The game's own
      `DisSeqAct_ShowHUDMessage` has no script handler, so it can't be called. Earlier idea: draw text
      from the DLL when the HUD renders (`HUD.PostRender` passes through
      `ProcessEvent`; the `Canvas` draw functions are callable the same way).
      Fallbacks: call into the Scaleform HUD, or hook Direct3D 9 and draw an overlay
- [ ] In-game message log and chat: keep a longer history in the DLL and show it on a
      hotkey (also while paused, e.g. over the pause menu); send chat from the game's own
      console (catch a command like `ap <text>` in the DLL and forward it to the client
      as `Say`), so no text box has to be built
- [ ] Connect without the Launcher, in stages: (1) server/slot/password in an ini and
      a client that auto-connects; (2) the DLL starts the client itself; (3) settings
      in the game's main menu. The menus are Scaleform (Flash) movies, so adding
      entries is the hard part (Crimsab's closed-source mod loader shows it's possible)
- [ ] Later, possibly: let the DLL connect to the Archipelago server itself
      (websocket + JSON in C++) so the external client becomes optional. The
      Python client stays the reference until then

### 8. Shopsanity (option): Piero's upgrades as checks
- [ ] apworld: one location per shop upgrade, behind an option
- [ ] DLL: refuse the real purchase (`Req_BuyItem`, already intercepted), take the coins,
      report a `shop check <upgrade>` event; nothing to undo because the effect is never
      applied. Refuse repeat purchases of checked locations (the client knows them)
- [ ] Rewrite the entry's name/description (`FDisStoreItem.m_Name`, `m_Description`)
      with the scouted item (LocationScouts); point `m_StoreIconName` at an existing
      icon. A custom Archipelago icon would need a new image in the Scaleform UI files

### Other languages and builds
- [ ] Confirm another game language works (Steam: Properties -> Language): detection, grants,
      blocking, zones and travel prompts all use internal names and paths, so they should;
      only log text (choice/travel text, upgrade display names) is translated. Check that the
      overlay draws non-ASCII Archipelago names (the engine's SmallFont may lack glyphs)
- [ ] Other builds (GOG, Epic, older patches) may not match the ProcessEvent byte pattern or
      the GNames/GObjects offsets; the log says so on startup

### Later
- Knife of Dunwall / Brigmore Witches, Dunwall City Trials, "smooth intro"
  (these are mostly data plus options in the apworld)
- DeathLink (already an option; the client needs to detect and cause deaths)

## Possible issues in the Manual data

- Fixed 2026-10-26: `Distillery District: Captain's Chair Hotel Bone Charm` (a House of
  Pleasure location) was in the `Distillery District (HOC)` region, but it is picked up in
  House of Pleasure's Distillery District. Moved to `Distillery District (HoP)`, and it needs
  the HoP Galvani's and Old Distillery keys: Slackjaw gives the hotel key after his jobs.

These are things to check, not changes I made:

- Fixed 2026-10-26: the `Southside Gate` region required the HoP Distillery, Galvani and
  Art Dealer keys but not `Southside Gate Zone Key`, so nothing needed that key. Now that the
  game enforces every zone's key, it's required too (stricter, so no seed gets harder).
- `Kennels Zone Key` exists as an item but has no matching region.
- Piero's Enhanced Arc Pylon blueprint (`BP_Piero_ArcPylon_AbsItm`, The Loyalists, from the
  `Pub07_ArcPylon` objective) isn't a location although the other blueprints are. Probably on
  purpose (it's for the non-lethal ending); a possible extra location, with greenranger's say.
- The goal location is the only one marked `"victory": true`. Mission victories
  such as `High Overseer Campbell: Victory` are normal checks.
