# Roadmap

The goal is to automate, one piece at a time, what the Manual makes the player do
by hand. Every step leaves something playable behind: anything not automated yet
stays on the Manual's honor system.

Status (2026-10-29): a full seed has been played to its goal with the DLL. Items are
granted and held to what was received, all 100 locations are detected, zone keys are
enforced, and Archipelago messages show in-game. What's left is verification (high chaos,
other choices, other builds), comfort features and new options.

## Architecture

```
Archipelago server  <-- websocket -->  Dishonored Client (Python)  <-- named pipe -->  dinput8.dll in Dishonored.exe
                                        worlds/dishonored/client/                       native/
```

- **apworld** (`worlds/dishonored/`): items, locations, regions and logic. It reads
  the same JSON as the Manual, and `data/ids.json` gives everything a fixed ID.
- **Client** (`worlds/dishonored/client/`): built on Archipelago's `CommonClient`.
  It only talks to the game through `GameInterface`: `native` (the DLL, the default) or
  `manual` (no game connection; `/check` and `/goal` by hand).
- **Game side** (`native/`): a `dinput8.dll` proxy that hooks `UObject::ProcessEvent`,
  reports game events, runs commands and enforces the client's limits.

## Milestones

### 0. Foundation (done)
- [x] Port the Manual's data to a real apworld with fixed IDs
- [x] Client with `/check` and `/goal`, which plays the same as the Manual
- [x] Tests and CI

### 1. Look inside the game (done)
- [x] Game build: Steam, `ProcessEvent` at exe+0x70640 (`research/notes.md`, Game build)
- [x] UnrealScript exported with UE Explorer; the classes behind pickups, powers, Piero's
      shop, travel prompts and mission ends are in `research/notes.md`
- [x] Console commands that survived the release build (`research/notes.md`)

### 2. In-game DLL (done)
- [x] `native/`: `dinput8.dll` proxy, `ProcessEvent` hook, named pipe to the client
      (several clients at once: the Archipelago client and `tools/native_console.py`)
- [x] Gameplay detection: real gameplay plus 3 s, not the main menu's background pawn
- [x] Call logging (`LogCalls=unique|filter`), F8 marker, `dump` for mapping new things
- [x] Events: `pickup`, `grant`, `objective`, `additem`, `choice`, `travel`, `area`, `mapchange`

### 3. Checks (done for one low-chaos playthrough)
- [x] Tell which rune, bone charm, painting or blueprint was taken, by the pickup's path
      in its level package, or its position when spawned at runtime (`near=`)
- [x] Mission victories and the goal (the journal entry written at each mission's end;
      the ending level `L_Out_P` for the goal). This replaced the Cheat Engine plan
- [x] All 100 locations have `detect` patterns, each seen in play (2026-10-29), with
      `research/locations.csv` filled in and `tools/check_log.py` to replay a log
- [x] Pickups a mission script removes far from the player are tagged `(far)` and never
      count; `!word` excludes events (Boyle's spare bedroom paintings, removed again on load)
- [ ] High chaos: Kingsparrow and the lighthouse load `_HighChaos_P` levels, and other
      missions have chaos variants (`L_Brothel_HighChaos`): replay those to confirm the
      same names match, or add theirs
- [ ] Choices not taken: e.g. killing both Boyle sisters (all three bedroom paintings
      are listed), Emily's other hiding spots (matched by the grant, so any spot should work)
- [ ] Reading a note produces no event yet (no location needs it so far)

### 4. Items (done)
- [x] Powers: granted with confirmed `AddPower` names, re-applied after loads
- [x] Powers: buying them with runes before they're received is refused (`PowerBuy`;
      its PowerId is lower case: "blink")
- [x] Upgrades (Piero's shop): the console cheats are stripped from the release build, so
      the DLL calls the pawn's `OnGiveUpgrade` with a borrowed `DisSeqAct_GiveUpgrade`
      (`upgrade <name>`). Names mapped in `client/item_commands.py`, including the second
      tier of Sword, Sleep Darts, Bullets and Grenades. Shop purchases of unreceived
      upgrades are refused and refunded
- [x] Bone charm slots: capacity = received slots. The DLL sets the base capacity to the
      slots received minus the bonus of every capacity upgrade owned (Piero's two and the
      four DLC statues); a negative base works. Covers mission-select snapshots and old saves
- [x] DLC statues: left alone (swapping their upgrade broke saves: the DLC gift code tracks
      them by upgrade name); their bonus is cancelled through the base above
- [x] Option for players without the DLC (`void_walkers_arsenal` off): 6 Bone Charm Slot
      items instead of 10
- [x] Equipment: bullets, bolts, sleep darts, grenades (+ explosive/incendiary/sticky),
      spring razors and the Rewire Tool are held at 0 until received (`limit ammo`,
      `limit item`). The sword is unequipped, taken out of the inventory and given back
      when received. "Corvo's Pistol" is the pistol *upgrade*: Corvo always has the pistol
- [x] Script and NPC grants of powers and upgrades that haven't been received are blocked
      (`OnAddPower`, `OnGiveUpgrade` in `ProcessEvent`; `blocked grant ...` events). Pickup
      and inventory grants (`OnGivePickup`, `OnAddInventoryItem`) are only reported as `grant`
      events; the equipment limits then take away what they gave. In-game door keys
      (`OnAddKey`) are not randomized and stay untouched
- [ ] Optional: block unreceived pickup/inventory grants up front too, instead of removing
      them afterwards
- [ ] Optional: refuse ammo purchases up front instead of refunding (shop entries:
      Bolt_Ammo_twk, SleepDart_Ammo_twk, Bullets_Store_Ammo_twk,
      SpringRazor_Ammo_WithItem_twk, Grenade_Ammo_WithItem_twk, RewireTool_twk)
- [ ] Show the mana bar when a power is received before the Void (the HUD hides it
      until the Outsider gives the mark). Level scripts toggle it with
      `DisSeqAct_ToggleHUDElement` (`m_HudElement` = `DHE_Mana`); find how that action
      takes effect and do the same from the DLL
- [ ] Optional: filter unreceived items out of the quick-select wheel instead of only
      holding their ammo at 0

### 5. Saves always match what was received
- [x] Items received after a save come back when that save is loaded: the client
      re-applies everything after each load and level transition (the Void swaps the
      inventory out and back)
- [x] Items *not* received are taken away when a save (or a mission-select snapshot) has
      them: powers through the pawn's `OnRemovePower`, upgrades removed from `m_Upgrades`,
      ammo and items zeroed. Confirmed from mission select: everything unreceived was
      removed within a second
- [ ] Removing an upgrade from `m_Upgrades` may not undo its effect (capacity is handled
      through the base; e.g. Silent Running unchecked)
- [ ] Known gap: loading a save *inside* the Void removes Blink there
- [ ] Optional: remember the seed in the save (or next to it) and warn when a
      save from another seed is loaded

### 6. Zone keys (done)
- [x] Map each zone to its streaming levels (`client/zones.py`): every mission confirmed in
      play (low chaos). The Flooded District's intro level and the ending have no zone
- [x] Refuse travel prompts into zones without their key ("Requires the X Zone Key"): prompts
      are `DisSeqAct_DialogScriptedChoice` actions whose options flag a map change; the DLL
      picks the "stay" option instead. Every prompt seen in play or in `dump` is in
      `TRAVEL_PROMPTS` (keyed `<action path>#<option>`, plus the area for level scripts two
      missions share). A mission's last prompt (e.g. the Flooded District's "Go to Hound
      Pits Pub") isn't refused, so its Victory never needs the next zone's key
- [x] Forced entries (story transitions, save loads, mission select, doors missing from
      the table) into keyless zones: the DLL reports `area <levels>` once settled; the client
      warns, then sends `quittomenu` 6 s later unless they walk out first, and the main menu
      shows why (`notice`)
- [x] Option `starting_zone_keys` (none / prison / intro) so the end of the Prologue
      doesn't depend on other players
- [ ] Show received zone keys in the journal's Key Ring tab: give a key through the
      pawn's native `OnAddKey` (`DisSeqAct_AddKey`: `m_Name`, `m_KeyTweaks`) like
      upgrades. Open question: can the entry show a custom name?
- Dropped: holding back checks found in a keyless zone until its key arrives, visible
  barriers, and pushing the player back. Sending them to the menu covers it

### 7. Option: missions as separate logic
- [ ] Today the regions are one chain of ~30 zone keys (Prologue -> ... -> Lighthouse), so
      late missions need nearly every earlier key (user idea, 2026-10-26). With mission
      select, each mission's first area could connect from the start and need only its own
      key; the Pub becomes its own area reached after any mission's end. Decision
      2026-10-26: keep the Manual's linear logic until the open questions are answered.
      Known so far: on a profile that has finished the game, mission select lists every
      mission, and a mission started from it restores the loadout from when it was first
      started; the client/DLL correct that to the received items, bone charm capacity
      included. Still open: whether an unfinished profile lists every mission
      (`DisGFxMoviePlayerMainMenu.OnMissionSelected(MissionIdx)` is native; the DLL may have
      to unlock them), and whether removed upgrades keep their effect (section 5)

### 8. In-game messages (towards an in-game client)
- [x] Show "Received X from Y" / "Sent X to Y" on screen: drawn with the HUD's Canvas
      after `HUD.PostRender` (`message <text>`, colour markers `{RRGGBB}`), bottom left in
      Archipelago's colours, hidden in menus
- [x] Show Archipelago chat in-game with the speaker's name
- [ ] Message history: keep a longer log in the DLL and show it on a hotkey (also while
      paused, e.g. over the pause menu)
- [ ] Send chat from the game: catch a console command like `ap <text>` in the DLL and
      forward it to the client as `Say`, so no text box has to be built
- [ ] Connect without the Launcher, in stages: (1) server/slot/password in an ini and
      a client that auto-connects; (2) the DLL starts the client itself; (3) settings
      in the game's main menu. The menus are Scaleform (Flash) movies, so adding
      entries is the hard part (Crimsab's closed-source mod loader shows it's possible)
- [ ] Later, possibly: let the DLL connect to the Archipelago server itself
      (websocket + JSON in C++) so the external client becomes optional. The
      Python client stays the reference until then

### 9. Shopsanity (option): Piero's upgrades as checks
- [ ] apworld: one location per shop upgrade, behind an option
- [ ] DLL: refuse the real purchase (`Req_BuyItem`, already intercepted), take the coins,
      report a `shop check <upgrade>` event; nothing to undo because the effect is never
      applied. Refuse repeat purchases of checked locations (the client knows them)
- [ ] Rewrite the entry's name/description (`FDisStoreItem.m_Name`, `m_Description`)
      with the scouted item (LocationScouts); point `m_StoreIconName` at an existing
      icon. A custom Archipelago icon would need a new image in the Scaleform UI files

### Other languages, builds and mod loaders
- [ ] Confirm another game language works (Steam: Properties -> Language): detection, grants,
      blocking, zones and travel prompts all use internal names and paths, so they should;
      only log text (choice/travel text, upgrade display names) is translated. Check that the
      overlay draws non-ASCII Archipelago names (the engine's SmallFont may lack glyphs)
- [ ] Other builds (GOG, Epic, older patches) may not match the ProcessEvent byte pattern or
      the GNames/GObjects offsets; the log says so on startup
- [ ] Load as an `.asi` (Ultimate ASI Loader) next to other mods: the DLL starts everything
      when loaded, so this is mostly a rename plus not proxying `dinput8` when loaded that way

### Later
- Knife of Dunwall / Brigmore Witches, Dunwall City Trials, "smooth intro"
  (these are mostly data plus options in the apworld)
- DeathLink (already an option; the client needs to detect and cause deaths)

## Possible issues in the Manual data

Fixed:

- 2026-10-26: `Distillery District: Captain's Chair Hotel Bone Charm` (a House of
  Pleasure location) was in the `Distillery District (HOC)` region, but it is picked up in
  House of Pleasure's Distillery District. Moved to `Distillery District (HoP)`, and it needs
  the HoP Galvani's and Old Distillery keys: Slackjaw gives the hotel key after his jobs.
  Seeds generated before this can lock the HoP Galvani's key behind the charm (seen 2026-10-29).
- 2026-10-26: the `Southside Gate` region required the HoP Distillery, Galvani and
  Art Dealer keys but not `Southside Gate Zone Key`, so nothing needed that key. Now that the
  game enforces every zone's key, it's required too (stricter, so no seed gets harder).
- 2026-10-26: the two Backyard bone charms were swapped. Rat Infested Building is the vise,
  which destroys a static `Ovrsr_NoPhysBoneCharm` and spawns a loose `Ovrsr_PhysBoneCharm`
  (matched by name and position); Overseer Stolen is `DisWhaleBoneCharm_0`.

To check, not changed:

- `Kennels Zone Key` exists as an item but no region needs it. The game enforces it, but the
  Kennels hold no locations, so it only gates an empty area (HOC was completed without it).
- Piero's Enhanced Arc Pylon blueprint (`BP_Piero_ArcPylon_AbsItm`, The Loyalists, from the
  `Pub07_ArcPylon` objective) isn't a location although the other blueprints are. Probably on
  purpose (it's for the non-lethal ending); a possible extra location, with greenranger's say.
- The goal location is the only one marked `"victory": true`. Mission victories
  such as `High Overseer Campbell: Victory` are normal checks.
