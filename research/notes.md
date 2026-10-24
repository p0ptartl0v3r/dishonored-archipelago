# Research notes

## Game build
- Store / version: Steam / 8335061
- `Dishonored.exe` size / hash: 66443f3d68a6c658b0ed943260c3eb2f4cc9e3400d5809a94d6ec41eb725e17e

## UnrealScript (UE Explorer)
Packages are in `Dishonored\DishonoredGame\CookedPCConsole`.

| What | Class / function | Notes |
|---|---|---|
| Any pickup | `ADisPickup_Base` | Base class of all pickups (from dismod's SDK, see prior-art.md) |
| Rune pickup | `ADisAbstractItemPickup`? | Abstract item = `UDisTweaks_PlayerPawn.m_pRuneItem`. Unverified |
| Bone charm pickup | `ADisWhaleBoneCharm` | |
| Painting / blueprint pickup | ? | Stat `ePlayerStat_SokolovPaintingFound` exists |
| Buy Outsider power | ? | `UDisPowerMenu`, cheat `AddPower` |
| Piero upgrade purchase | ? | `ADisUpgrade`, `UDisTweaks_Upgrade`, cheat `AddUpgrade` |
| Map load / transition | | |
| Mission end screen | `UDisSeqAct_ShowMissionStats`? | `UDisGFxMoviePlayerMissionStats` |

## Packages worth opening (CookedPCConsole)

| File | Why |
|---|---|
| `DishonoredGame.upk` | The game's own UnrealScript classes. **Start here.** |
| `Engine.upk`, `GameFramework.upk`, `Core.upk` | Stock UE3 base classes that DishonoredGame builds on |
| `Twk_Upgrades_SF.upk` | Probably the upgrade definitions behind Piero's shop |
| `Blueprints_twk_Abst_SF.upk`, `pickups_twk_Keys_SF.upk`, `Money_twk_Abs_SF.upk` | Probably item/pickup definitions |
| `*_AbstractInv_MS_SF.upk` | One per mission: probably that mission's inventory/pickup definitions |
| `L_*_PickUps.upk`, `L_Pub_Pickups.upk`, `L_Distillery_Loot.upk` | Placed pickups: source for `game_object` in locations.csv |
| `UI_MissionStats_SF.upk` | End-of-mission stats screen: a possible mission-complete signal |
| `UI_Shop_SF.upk`, `UI_PowerWheel_SF.upk` | Shop and power UI |

Skip `.pck`/`.bnk` (Wwise audio) and `.tfc` (textures).

Level packages: `L_<Name>_P.upk` is the *persistent* map, which is what the
engine reports as the current map. The other `L_<Name>_*` files (Audio, Nav, FX,
Script...) are sublevels streamed into it.

## Persistent maps -> zones

The guesses come from file names only. Confirm each one in-game (load the area,
read the map name in Cheat Engine or the console) and change "?" to "yes".

| Persistent map | Zone (regions.json) | Confirmed |
|---|---|---|
| `L_Tower_P` | Prologue | ? |
| `L_Prison_P` | Dunwall Prison | ? |
| `L_PrsnSewer_P` | Dunwall Sewers | ? |
| `L_Pub_Morning_P` / `_Day_P` / `_Dusk_P` / `_Night_P` | Hound Pits Pub (time of day changes per visit) | ? |
| `L_OutsiderDream_P` | The Void | ? |
| `L_Streets1_P` | Distillery District (HOC) | ? |
| `L_Distillery_P` | Old Dunwall Distillery (HOC) | ? |
| `L_Galvani1_P` | Dr Galvani's Offices (HOC) | ? |
| `L_Ovrsr_P` | High Overseer's Office | ? |
| `L_Ovrsr_Back_P` | Backyard | ? |
| `L_Ovrsr_Kennel_P` | Kennels (has a zone key but no region yet) | ? |
| `L_Streets2_P` | Distillery District (HoP) | ? |
| `L_Distillery2_P` | Old Dunwall Distillery (HoP) | ? |
| `L_Galvani2_P` | Dr Galvani's Offices (HoP) | ? |
| `L_ArtDealer_P` | Art Dealer's Apartment | ? |
| `L_Brothel_P` | The Golden Cat Bathhouse | ? |
| `L_Bridge_Part1a_P`, `_Part1b_P`, `_Part1c_P`, `_Part2_P` | Southside Gate / Drawbridge Control / Midrow Substation / North End (order unknown) | ? |
| `L_Boyle_Ext_P` | Boyle Mansion Exterior | ? |
| `L_Boyle_Int_P` | Boyle Mansion Interior | ? |
| `L_TowerRtrn_Yard_P` | Tower Exterior | ? |
| `L_TowerRtrn_Int_P` | Tower Interior | ? |
| `L_Flooded_FIntro_P` | Rudshore Waterfront? | ? |
| `L_Flooded_FStreets_P` | Central Rudshore? | ? |
| `L_Flooded_FRefinery_P` | Greaves Refinery | ? |
| `L_Flooded_FAssassins_P` | Daud's base (no region of its own?) | ? |
| `L_Flooded_FGate_P` | Rudshore Gate | ? |
| `L_Streetsewer_P` | Old Dunwall Sewers? | ? |
| `L_Pub_Assault_P` | Hound Pits Pub (The Loyalists) | ? |
| `L_Isl_LowChaos_P` / `L_Isl_HighChaos_P` | Kingsparrow Fort | ? |
| `L_LightH_LowChaos_P` / `L_LightH_HighChaos_P` | Burrows Lighthouse | ? |
| `L_Out_P`, `L_Out_HighChaos_*_P` | Ending ("Out" = outro?). **Possible goal signal** | ? |
| `Dishonored_MainMenu` | Main menu | ? |

Other notes:
- `L_Pub_From<Mission>_Script` packages: the pub loads a different script per
  mission you return from, so the pub's map plus that script tells you which
  mission just finished.
- Chaos variants (`_HighChaos`/`_LowChaos`) mean the client must treat
  several map names as the same zone.

## Console commands

| Command | Effect |
|---|---|
| `giverunes N` | Adds N runes. **Works** |
| `AbstractInventoryList` | Nothing on screen. Native; check `Logs\Launch.log` / `-log`, may be compiled out |
| `AddPower <Name> <Level>` | **Works** (levels 1 and 2). Names from the command's own in-game list: `Blink`, `DarkVision`, `DevouringSwarm`, `Possess`, `BendTime`, `WindBlast`, `Vitality`, `BloodThirsty`, `Celerity` (= Agility), `ShadowKill`, plus others that are probably Daud/DLC powers. Re-applying on every load works (Blink 1+2 restored after reload) |
| `AddUpgrade <Name>` | ? |
| `GiveBoneCharm` | ? |
| `RemovePower <Name>` | ? (needed to undo powers bought early) |
| `MaxUpgrades` | "Command not recognized", although autocomplete offers it. Same function flags in the SDK as the working `GiveRunes`, so the difference is inside the exe (probably compiled out of the release build) |
| `MinUpgrades`, `AddUpgrade <Name>`, `RemoveUpgrade <Name>` | ? Do these also say "not recognized"? |
| `GivePlayerItem <TweakString>` | ? |

Autocomplete lists every `exec` function declared in script, including ones
whose native code was removed, so "suggested" doesn't mean "works".

Fallback when a cheat is missing: the game gives things through Kismet actions
that level scripts use, which must exist in the release build:
`UDisSeqAct_GiveUpgrade` (`m_pUpgrade`: a `UDisTweaks_Upgrade`),
`UDisSeqAct_AddPower` (`m_PowerName`, `m_PowerLevel`),
`UDisSeqAct_GivePickup` (`m_pPickup`), handled by the player pawn's
`OnGiveUpgrade` / `OnAddPower` / `OnGivePickup`. A native DLL can call those
through `ProcessEvent`.

## Native module findings

- Steam build: `ProcessEvent` at exe+0x70640 (found by pattern); dismod's
  GNames/GObjects offsets are valid for it.
- `PlayerTick` doesn't exist as a script function in Dishonored (moved to native
  code), so the DLL finds the player via `DishonoredEngine.GamePlayers[0].Actor`.
- Cheat commands only work once `PlayerController.CheatManager` (+0x45C) exists.
  Enabling the console in-game (F1 with the console unlocker) creates it.
  Calling `PlayerController.EnableCheats` directly through ProcessEvent right
  after loading did **not** create it, nor did running it as a console command:
  `PlayerController.CheatClass` (+0x460) is **None** in the shipped game, so
  `EnableCheats` has nothing to instantiate. The F1 console unlocker evidently
  sets it. The DLL now sets CheatClass to `Class DishonoredGame.DishonoredCheatManager`
  before running EnableCheats (unconfirmed).
- Setting CheatClass to `DishonoredCheatManager` then running `EnableCheats`
  works: confirmed in-game, no F1 needed.
- The main menu's background scene also has a player pawn, and a loaded save
  keeps restoring itself for ~0.5s after the pawn appears (its `(load)` pickup
  events come after). `AddPower` sent in either window was lost, so the DLL
  now waits for real gameplay (not the menu) plus 3 seconds.
- `L_Ovrsr_Kennel_P` is its own streaming level (mapchange from the office):
  the Kennels zone key could gate it.
- `CheatClass`/`CheatManager` are per controller: a new one after every load, so
  cheats have to be re-enabled after each load (the DLL does this).
- The client must not mark an item delivered unless the command can work:
  items now wait for `pong ready` (cheats on), and unverified power names are
  shown to the player instead of being sent.

### From the first call log (unique mode)
- F1 / console unlock = `Engine.PlayerController.EnableCheats`.
- Loading a save: `OnLoadGameConfirm` -> `PreClientTravel` -> `GameEnding`, then the
  new level. Pickups already collected are removed with `Destroyed` right after
  load (`KActor.Destroyed on DisStatPickup`).
- Rune-related: `DishonoredPawn.OnAddAbstractItem` (scripted item gift),
  `KActor.SpawnedByKismet on DisAbstractItemPickup` (a pickup spawned by a level
  script), `DishonoredPlayerController.OnObjectiveAction`, `DishonoredPlayerPawn.OnAddKey`.
- Map changes: `PlayerController.ClientPrepareMapChange` / `ClientCommitMapChange`.
- Death: player death shows up as the pause menu's `OnLoadLastSaveClicked` after
  `NotifyTakeHit`; NPC deaths call `GameInfo.NotifyKilled` / `DishonoredPawn.PlayDying`.
- Unique mode hides repeats (only the first rune pickup appears), so the DLL now
  reports pickups, map changes, scripted items and objectives as events instead.

### From the first event log (Distillery District -> Overseer's office)
- Every level streams into one persistent map, `DishonoredGameFull_P`. The
  map-to-zone table above is about **streaming** levels, which `mapchange` names
  (`mapchange L_Ovrsr_P` on entering the office).
- Placed pickups belong to the level's *script* package and have stable names:
  `L_Streets1_Script.TheWorld.PersistentLevel.DisAbstractItemPickup_19`.
- Some runes are spawned at runtime (`DishonoredGameFull_P...DisAbstractItemPickup_64`);
  match those by position (`near=`).
- Item names: `Rune_AbsItm`, `Coins_AbsItm`, bone charms `WhaleBoneCharm_01_twk`
  (the tweak name is shared, so it doesn't identify the charm; the path does).
- Objectives: `DisSeqAct_AddObjective` / `DisSeqAct_CompleteObjective` with names like
  `Optional_HelpGrannyRags`, `Optional_GrannyRagsHook`, `Ovrsr_KillOverseerCambell`,
  `Ovrsr_GetBlackBook`. Good candidates for bonus and mission checks.
- Loading a save (from the menu or after death) rebuilds the world without a
  `mapchange`, so after a load the current zone isn't reported. Only walking
  through a transition (Distillery -> office) produces `mapchange`. Zone keys will
  need the list of loaded streaming levels after a load (WorldInfo.StreamingLevels).
- Reading a note (Griff's shop bone charm note) produced no event: notes aren't
  destroyed when read. Needs its own hook if notes become checks.
- Scripted items: `TheStill`, `GriffShop`, `ChapterNotes_Overseer`,
  `OvrsrChptrNote_Poisoning` (journal notes/keys, not runes).
- `DishonoredInventoryPickup` / `DisStatPickup` events are ammo, elixirs and
  dropped items: noise for our checks.

### Player state (from the SDK, read by the DLL's `dump`)
- `DishonoredPlayerPawn.m_Upgrades` (+0xB50): owned `DisTweaks_Upgrade`s.
  `OnGiveUpgrade(DisSeqAct_GiveUpgrade)` is native and reads the action's `m_pUpgrade` (+0xF8).
- `DishonoredPlayerPawn.m_WhaleBoneCharmSlots[10]` (+0xB10). The number of usable slots
  looks like an upgrade attribute: `Twk_Upgrades_SF.upk` mentions
  `Attribute_MaxActivatedBoneCharmCount`, and `DisTweaks_PlayerPawn` has
  `m_MaxActivatedBoneCharmCount` and `m_BoneCharmSlotDLCUpgrades` (10 slots with DLC, 6 without).
- `DishonoredPawn.m_pInventory` (+0x59C) -> `DishonoredInventory`: `m_Slots` (equipped items),
  `m_AmmoInfo` (count, capacity per `EeDisAmmoType`: Bullet, Bullet_Explosive, Arrow,
  Arrow_Sleep, Arrow_Flare, SpringRazor, Grenade, StickyGrenade, ...), `m_AbstractItem`
  (item + quantity: runes, coins, blueprints?).
- Controller.Pawn is the possessed creature while possessing, so the DLL checks the pawn
  is a `DishonoredPlayerPawn` before touching player state.

### First `dump` (2026-10-19, save at the start of The Flooded District)
- Upgrade object names (`Twk_Upgrade_*`) are all loaded during gameplay; the mapping
  to items is in `client/item_commands.py`. Not randomized: `Twk_Upgrade_Mask`
  (Corvo's Mask).
- Bone charm capacity (from the user): Corvo starts with 3 slots; `Twk_Upgrade_BoneCharms`
  adds 1 and `Twk_Upgrade_BoneCharms2` adds 2 (Piero); each DLC statue upgrade
  `Twk_Upgrade_Statue{Fish,Whale,Wolfhound,Rat}` ("Mystical ... Artifact", from the
  Acrobatic Killer / Arcane Assassin / Backstreet Butcher / Shadow Rat packs in Void
  Walker's Arsenal) adds 1: 10 in total, the Manual's 10 slot items. With both Piero
  upgrades owned, six slots were filled. `m_WhaleBoneCharmSlots` holds the equipped charm's info index
  per slot, -1 = empty.
- Inventory slots are fixed per equipment type (0-1 empty hands, 2 sword, 3 powers,
  4 crossbow, 5 pistol, 6 spring razor, 7 grenade, 8 heart). At the start of the
  Flooded District (Daud takes Corvo's gear) slots 2 and 4-7 were None, so "no sword"
  is representable as an empty slot.
- Ammo capacities with upgrades: Bullet 30, Bullet_Explosive 10, Arrow 30,
  Arrow_Sleep 10, Arrow_Flare 10, SpringRazor 9, Grenade 9, StickyGrenade 9.
- Abstract items include notes/journal entries, `Rune_AbsItm` (unspent runes),
  `Coins_AbsItm`, `BoneCharms`, `RewireTool` (the Rewire Tool is an abstract item, not
  equipment), and blueprints as `BP_*_AbsItm` (`BP_SilentRunning_AbsItm`,
  `BP_SpyglassZoom_AbsItm`, `BP_StickyGrenade_AbsItm`, `BP_HealthPotion_AbsItm`,
  `BP_FlareBolt_AbsItm` = incendiary bolts). Blueprint checks should show up as
  `pickup ... BP_..._AbsItm` events.

### First-mission playthrough (2026-10-24)
- **New Game** from the menu loads `l_tower_p` without the pawn ever disappearing, so
  the old "lost the pawn, then regained it" menu detection never fired (no items, no
  inventory diff for the whole session). The DLL now treats a mapchange to a real
  level while in the menu as a new game and waits for the controller/pawn to change.
- Safe loot (Jelly's safe) and containers produce no `pickup` event: the items never
  existed as actors. The inventory diff (`gain item`) is the way to see them.
- The pub's DLC gifts (6 + 6 bone charms, Mystical Fish/Rat artifacts) appear when
  you first sleep at the pub: spawned `DisWhaleBoneCharm` pickups in
  `DishonoredGameFull_P`. The statues produced no pickup event (probably `ADisUpgrade`
  actors, not `DisPickup_Base`); they give bone charm slots in vanilla, so they need
  blocking like Piero's upgrades.
- In the prison, equipment pickups (City Watch sword, pistol) are
  `DishonoredInventoryPickup`s with no item name in the event (`?`).
- `TowerEmpress_HideNSeek` is completed when you win and *failed* twice later, so
  match only `DisSeqAct_CompleteObjective`.

## Memory (Cheat Engine)

| Value | Module + pointer path | Type | Notes |
|---|---|---|---|
| Current map name | | | |
| Loading flag | | | |
| Rune count | | | |
