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
| `L_Tower_P` | Prologue | yes (`l_tower_p`) |
| `L_Prison_P` | Dunwall Prison | yes |
| `L_PrsnSewer_P` | Dunwall Sewers | yes |
| `L_Pub_Morning_P` / `_Day_P` / `_Dusk_P` / `_Night_P` | Hound Pits Pub (time of day changes per visit) | yes (+ `L_Pub_From<Mission>_Script[_Sleep]`) |
| `L_OutsiderDream_P` | The Void | yes |
| `L_Streets1_P` | Distillery District (HOC) | yes |
| `L_Distillery_P` | Old Dunwall Distillery (HOC) | yes |
| `L_Galvani1_P` | Dr Galvani's Offices (HOC) | yes |
| `L_Ovrsr_P` | High Overseer's Office | yes |
| `L_Ovrsr_Back_P` | Backyard | yes |
| `L_Ovrsr_Kennel_P` | Kennels (has a zone key but no region yet) | yes |
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
- Safe loot is ordinary placed pickups: Jelly's safe holds `DisElixirHealth_1` and a
  valuable (`DisAbstractItemPickup_79`, `Coins_AbsItm`, worth 50). Valuables turn into
  coins when taken. (An earlier guess that safes give no event came from misreading
  which F8 marks belonged to the safe.)
- The pub's DLC gifts (6 + 6 bone charms, Mystical Fish/Rat artifacts) appear when
  you first sleep at the pub: spawned `DisWhaleBoneCharm` pickups in
  `DishonoredGameFull_P`. The statues produced no pickup event (probably `ADisUpgrade`
  actors, not `DisPickup_Base`); they give bone charm slots in vanilla, so they need
  blocking like Piero's upgrades.
- In the prison, equipment pickups (City Watch sword, pistol) are
  `DishonoredInventoryPickup`s with no item name in the event (`?`).
- Second run: New Game left the menu only through the 20 s fallback (the menu's
  controller and pawn carry over). Harmless.
- **The Void swaps the inventory**: entering `L_OutsiderDream_P` removed every note and
  `Twk_Upgrade_Mask`; returning to the pub restored them within 0.1 s of the mapchange.
  Anything granted in between would be lost, so every mapchange now counts as a reset
  (`gameplay` event, 3 s settle, client re-applies all items).
- Script grants seen: prologue `Twk_Inv_PistolEliteGuard_Player`,
  `Twk_Inv_SwordEliteGuard_Player_Intro`; sewer gear cutscene `Twk_Inv_SwordCorvo`,
  `Twk_Inv_CrossbowCorvo` (Corvo_SpecialGear); Void `grant power Blink 1`,
  `Twk_Inv_HeartCorvo`; pub `grant upgrade Twk_Upgrade_Mask`. Tutorials arrive as
  `grant pickup *Tutorial_PickUp` (noise).
- Third run (connected to a seed): all detected checks reached the server; Windblast
  was granted mid-prison; the Void removed a received upgrade and the client
  re-granted it, and the game's own restore on return found it "already owned".
- Piero's rune is **bought** (500 coins). Its pickup then spawns at a different spot
  each run, so it's detected with the `buy` event (coins down + something up in one
  reading) instead of by position.
- The DLC packs at the pub add 4 × 500 coins, `DLC_Only_*` notes and the four
  `Twk_Upgrade_Statue*` upgrades (seen as `gain upgrade`, no pickup event).
- In-game door keys (`DisKey_Base_*`) stay vanilla, as in the Manual.
- `TowerEmpress_HideNSeek` is completed when you win and *failed* twice later, so
  match only `DisSeqAct_CompleteObjective`.

### Powers (2026-10-24)
- Passive powers are entries in `DishonoredPowersComponent.m_Powers` (Vitality,
  BloodThirsty, ShadowKill, Celerity; `m_CurrentLevel` -1 = not owned). Active powers
  (Blink, ...) are each a `DishonoredActivePowerComponent` object with `m_PowerName`
  (+0x6C), `m_pOwnerPlayerPawn` (+0x80) and `m_CurrentLevel` (+0x88); the DLL finds the
  player's by scanning the object table.
- `AddPower <Name> <Level>` sets the level absolutely: a save with Blink 2 bought came
  back as Blink 1 after the client re-applied "AddPower Blink 1". So lowering a power is
  just AddPower with the lower level; only "none" needs `OnRemovePower`.
- `PowerBuy` is a native function of `DisGFxMoviePlayerJournal` (the journal's power
  page). Whether its calls from the Flash menu pass through ProcessEvent is still
  unverified.
- Removing an upgrade from `m_Upgrades` hides it from the journal but keeps its effect:
  bone charm capacity stayed at 8 after the statues and Piero's capacity upgrade were
  removed, also after a reload. Upgrades carry `m_Attributes` (name, value); the pawn's
  `DisAttributes` keeps named modifiers in a native map. The exe has the string
  "Upgrade" (candidate modifier name) and "TRANSITION-LOAD : Upgrade %d of %d".
- The shop: `DisGFxMoviePlayerStore.Req_BuyItem(ItemIdx)`; `m_DisplayedItems` (+0x1C4)
  holds pointers to `FDisStoreItem` (`m_Item` +0x5C): confirmed, blocking a purchase
  there works and nothing is applied.
- `OnRemoveAttributeModifier` on `Attribute_MaxActivatedBoneCharmCountBonus` with the
  modifier names `Upgrade`, `Twk_Upgrade_StatueFish`, `Twk_Upgrade_BoneCharms` changed
  nothing (2026-10-25). Upgrade effects are still not undoable; the approach is to
  prevent them instead (shop blocking; destroying `DisUpgrade` statues before pickup).
- The statues are `DisUpgrade` actors (`m_pTweaks` +0x254) with no script functions:
  pickup is native, so it can't be refused through ProcessEvent. In-game: `Destroy`
  returns false (placed in the level), `SetHidden` works, `SetLocation` doesn't move
  them, and a hidden, non-colliding statue can still be picked up.
- **Don't change a statue's upgrade.** `DisTweaks_PlayerPawn.m_DLCPackInfo` lists each DLC
  pack's statue by upgrade name (`Twk_Upgrade_StatueRat`, ...) with flags
  `Spawned_DLC_Statue<n>` / `Got_DLC_Statue<n>` (likewise for its charms, book and coin
  pouch). After the DLL pointed statues at `Twk_Upgrade_Mask`, a pub save made after
  waking up crashed on load ("Assertion failed: i>=0 && (i<ArrayNum ...)", Array.h:596,
  also without the DLL) and the game crashed ~12 s after waking (2026-10-25). The
  autosave made on going to sleep still loaded. The swap is stored in saves.
- UE3 crash reports land in `Documents\My Games\Dishonored\DishonoredGame\Logs`
  (`UE3AutoReportDump*.txt` and a minidump per crash); Launch.log stays empty.
- Equipment: holding unreceived ammo types and the Rewire Tool (`RewireTool_AbsItm`) at 0
  works, magazines included (`DishonoredWeapon_Ranged.m_LoadedAmmoInfo`; the grenade
  launcher is one too). Shop entries: Bolt_Ammo_twk, SleepDart_Ammo_twk,
  Bullets_Store_Ammo_twk, SpringRazor_Ammo_WithItem_twk, Grenade_Ammo_WithItem_twk,
  Elixir_Mana_twk, RewireTool_twk, Rune_twk.
- Sword: calling `OnEquipItemType` (DishonoredItemEmpty, primary hand) with the borrowed
  default action did nothing, every second, in the sewers and in Coldridge. With its
  "Apply" input marked (`InputLinks[0].bHasImpulse`) it works, but attacking or
  blocking draws the sword straight back. So the DLL now also clears the sword's
  inventory slot once no hand holds it, and gives `Twk_Inv_SwordCorvo` back through
  `OnAddInventoryItem` when Progressive Sword arrives. Both confirmed in-game.
- A long-running Launcher keeps the client code it started with: restart it after
  client changes.

### Zones and transitions (2026-10-25)
- The current zone after a save load: `WorldInfo.StreamingLevels` (+0x314) lists
  `LevelStreaming` objects (`PackageName` +0x38, `LoadedLevel` +0x40, `bIsVisible` bit 0
  of +0x5C). After every load and transition the area's `L_..._P` level and its
  sublevels were the loaded, visible ones (Streets1, Galvani1, Distillery, Ovrsr).
- A travel prompt is a conversation player choice: `Dis_PlayerChoice_RequestSkip`, then
  `DisGFxMoviePlayerHUD.OnPlayerChoiceConfirm(Index)`, then `mapchange` 22 ms later.
  Dialogue answers use the same function. The active choice node is
  `StatePlayerMasterInDialog.m_pActivePlayerChoice` (+0x64), a `DisConv_PlayerChoice`
  with its texts in `m_Choices` (+0xA0). The DLL now reports `choice <index> "<text>" by
  <node path>` to learn which choices are travel prompts. That state held nothing for
  travel prompts: they are **scripted** choices, `StatePlayerMasterInScriptedChoice
  .m_pActiveScriptedChoice` (+0x5C), a level-script `DisSeqAct_DialogScriptedChoice` with
  `m_Title` (+0x10C) and `m_Choices` (+0x118; text, then flags with bit 0
  `m_bDisablePlayerInputUntilMapChange` = this choice travels). The wine (HOC), the
  still (Old Dunwall Distillery) and the donation box are scripted choices too.
- `disconnect` is stripped from the release build ("unrecognized command").
  `DisGFxMoviePlayerPauseMenu.OnQuitGameConfirm` on the pause menu object returns to the main
  menu: confirmed (`quittomenu`). The object always exists
  (`DishonoredGameInfo.pGlobalUIManager.pPauseMenu`), even if the pause menu was never opened.
- Quitting to the menu drops the player pawn *before* the `Dishonored_MainMenu` mapchange,
  then the menu's background pawn appears ~50 ms later. Only a pawn lost after that mapchange
  means a save is loading.
- Travel prompts are `DisSeqAct_DialogScriptedChoice` actions in the level scripts; options
  flagged `m_bDisablePlayerInputUntilMapChange` travel. Their output 0 is "Player Busy", then
  one per option (named "[<option text>]"). The chosen option leads to
  `DisSeqAct_SetPlayerTravelDestination.m_Tag` (a PlayerStart name in the destination level,
  e.g. `PlayerStart_DistilleryMainEntrance`), sometimes through remote events and nested
  sequences. The destination level isn't loaded beforehand, so the tag can't be resolved
  to a level at runtime: options are mapped by hand (`client/zones.py`). Changing the
  confirmed index to a non-travel option before `OnPlayerChoiceConfirm` runs declines the
  prompt cleanly (confirmed). One area can have several doors to the same place (Galvani's
  has three; the High Overseer's Office has three prompts for the Back Yard, one never
  found in play).

## Memory (Cheat Engine)

| Value | Module + pointer path | Type | Notes |
|---|---|---|---|
| Current map name | | | |
| Loading flag | | | |
| Rune count | | | |

- Bone charm capacity = base (`m_MaxActivatedBoneCharmCount`) + the bonuses of owned capacity
  upgrades, and the base may be negative: with 2 slots received and all four statues picked up,
  base -2 kept 2 slots (2026-10-26). Saves are in `Steam\userdata\<id>\205100\remote`:
  `DisMission<n>.sav` are the mission-start snapshots mission select loads, `DisAutoSave*` the autosaves.

### House of Pleasure (2026-10-26)
- Levels: `L_Streets2_P` (Distillery District), `L_Distillery2_P`, `L_Galvani2_P`, `L_ArtDealer_P`,
  `L_Brothel_P` (+ `L_Brothel_HighChaos`). Galvani's offices reuse `L_Galvani1_Scripts` (its
  travel prompts) plus `L_Galvani1_Strt2_Scripts` (this mission's pickups), so the same prompt
  leads to a different Distillery District in each mission.
- Paintings are valuables (`DisAbstractItemPickup` of `Coins_AbsItm`, 300 coins each). Looking
  at one adds a journal entry (`AbsInv_ArtDealer_Painting06`, ...), which isn't the pickup.
- The Art Dealer's safe opens without any event; its check is the rune inside.
- Mission end: talking to Samuel removes every objective in one `DisSeqAct_RemoveObjective`
  (all names in one word), then `additem ... fromStreets2_toHub ... ChapterLog_Hub_03`.
- `Captain's Chair Hotel Bone Charm` is `L_Streets2_Script...DisWhaleBoneCharm_4`, this mission.

### The Royal Physician (2026-10-26)
- Four bridge levels: `L_Bridge_Part1a_P` (Southside Gate), `L_Bridge_Part1b_P` (Drawbridge
  Control), `L_Bridge_Part1c_P` (Midrow Substation), `L_Bridge_Part2_P` (North End); scripts
  `L_Bridge_Part1a_Script` etc. Mission end: `additem ... fromBridge_toHub ... ChapterLog_Hub_04`.
- Sokolov's safehouse painting is a 300-coin valuable like the Art Dealer's.
- "Hound Pits Pub: Abandoned Apartment Bone Charm" is in the pub after this mission
  (`L_Pub_Night_P`), spawned at runtime (`DishonoredGameFull_P...DisWhaleBoneCharm_5`).
- Folded Galvani Resin is `BP_SilentRunning_AbsItm`.

### Lady Boyle's Last Party (2026-10-27)
- Levels `L_Boyle_Ext_P` (streets and grounds) and `L_Boyle_Int_P` (mansion); pickups in
  `L_Boyle_Ext_Script`, `L_BoyleStreet_Ext_Script`, `L_Boyle_Int_Script`, and the shrine bone
  charm in `L_Boyle_Ext_Audio`. The travel prompts are in the `_P` packages themselves
  (`L_Boyle_Ext_P...DialogScriptedChoice_4` foyer, `_0` cellars; `L_Boyle_Int_P..._1` sewers).
  Mission end: `additem ... fromBoyle_toHub ... ChapterLog_Hub_05`.
- About 5 s after the mansion loads, its script destroys pickups it doesn't use (another
  bedroom's painting `_68`/`_69`, two keys) thousands of units from the player; the exterior
  does the same. These looked like pickups, so the DLL now tags a pickup destroyed more than
  1000 units from the player `(far)` and the client ignores it (real pickups: at most ~270).
  There are three bedroom paintings (`L_Boyle_Int_Script` `_67`, `_68`, `_69`, one per sister)
  and the script keeps one. Coming back into the mansion, the save removes the other two again
  *while loading*, like collected pickups, so the Bedroom Painting matches any of the three but
  never a `(load)` event (detect word `!(load)`).
- The three "The Pub:" runes are in the pub afterwards (`L_Pub_Morning_P`): Spare a Boyle is
  placed (`L_Pub_FromBoyle_Script...DisAbstractItemPickup_0`, next to a note). Find Emily's rune
  spawns where Emily hid (a different spot each run) and the duel's is spawned too; both are
  matched by the action that gives them: `grant pickup Rune_twk by
  L_Pub_FromBoyle_Script...DisSeqAct_GivePickup_0` (Emily) and `_2` (duel), same in two runs.

### Return to the Tower (2026-10-27)
- Levels `L_TowerRtrn_Yard_P` (grounds, and the rooftop) and `L_TowerRtrn_Int_P`; pickups in
  `L_TowerRtrn_Yard_Script` / `L_TowerRtrn_Int_Script`. Travel prompts from `dump`: the yard's
  `DialogScriptedChoice_3/5/6/7` go inside; the interior's `0/7/8` go to the exterior and `5`
  to the rooftop (which loads `L_TowerRtrn_Yard_P`, so it needs the Tower Exterior key).
- Mapped from the marks without a list: the Safehouse Rune is the yard rune on the rooftop
  (`_6`), the Bunkhouse Rune the other one (`_47`). Small-scale Combustion Refinement is
  `BP_ExpBullets_AbsItm`. Mission end: `additem ... fromTowerReturn_toHub ... ChapterLog_Hub_06`
  as the boat reaches the pub (`L_Pub_Dusk_P`, `L_Pub_FromTwrReturn_Script`).

### The Flooded District (2026-10-29)
- Levels: `L_Flooded_FIntro_P` (where Corvo wakes; no zone of its own), `L_Flooded_FStreets_P`
  (Rudshore Waterfront), `L_Flooded_FRefinery_P` (Greaves Refinery), `L_Flooded_FAssassins_P`
  (Central Rudshore: Daud's base), `L_Flooded_FGate_P` (Rudshore Gate). Pickups are in
  `<level>_PickUps` packages (plus `L_Flooded_FAssassins_Script`). Travel prompts are in the `_P`
  packages; "Go to Rudshore Market" from Daud's base is assumed to lead to the Waterfront.
- Retrieve your Gear = `objective ... CompleteObjective Flooded_Gear`; the game then gives back
  every item and journal entry at once (one `gain item` line each). The sword stayed away until
  then (the DLL didn't give it back early).
- Daud's Bone Charm is handed over by `L_Flooded_FAssassins_Script...Daud_Behavior.DisSeqAct_GivePickup_1`,
  the same action whether Daud is killed or spared (both seen).
- `L_Streetsewer_P` is the Old Dunwall Sewers (pickups in `L_Streetsewer_Script` and
  `L_Streetsewer_Block`), reached from Rudshore Gate's "Go to the Old Port District Outskirts".
  Granny's shrine rune is spawned (`DishonoredGameFull_P`) as `Strt3Obj_HelpGranny` completes,
  matched by position. Mission end: "Go to Hound Pits Pub" (`L_Streetsewer_Script` choice 4),
  then `additem ... L_Pub_FromFlooded_Script ... ChapterLog_Loyalists` in `L_Pub_Assault_P`.
  That prompt isn't refused (the Victory needs no pub key).

### The Loyalists (2026-10-29)
- All in `L_Pub_Assault_P`, pickups in `L_Pub_FromFlooded_Script`. Piero's Workshop Rune is
  `DisAbstractItemPickup_217`; the Enhanced Arc Pylon blueprint (`_216`,
  `BP_Piero_ArcPylon_AbsItm`) isn't a location. The boat to Kingsparrow is a story transition
  (no prompt); mission end: `additem L_Isl_Script_Slave ... ChapterLog_Lighthouse` as it lands
  (`L_Isl_LowChaos_P`).

### The Light at the End (2026-10-29, low chaos)
- `L_Isl_LowChaos_P` (Kingsparrow Fort, script `L_Isl_Script_Slave`), then the player-taken
  prompt "Go to Burrows Lighthouse" (`L_Isl_Script_Slave` choice 0) to `L_LightH_LowChaos_P`
  (script `L_LightH_Script_Slave`). The ending loads `L_Out_P` plus `L_Out_Env*` sublevels
  chosen by what happened (`L_Out_EnvSamAlive`, ...): `mapchange L_Out_P` is the goal.
- Drainage Rune is `L_Isl_Script_Slave...DisAbstractItemPickup_30`. High chaos loads
  `_HighChaos_P` levels; whether their pickups and prompts share these names is unchecked.
- With this, all 100 locations have detect patterns (every mission played at least once).

### Item meanings: read the Manual's PDF (`manual/Dishonored_Archipelago_Documentation.pdf`)
- "Corvo's Pistol" is the *Corvo's Pistol Upgrade* from Piero's shop (`Twk_Upgrade_Pistol`),
  not the pistol: Corvo always has a pistol. Progressive Bullets 1 = buy and use bullets,
  2 = explosive bullets. (A build that took the pistol away was reverted, 2026-10-26.)
- Progressive Sword 1 = use the sword at all, 2 = Sword Crossing. Sleep Darts 2 = the Combat
  Sleep Darts upgrade. Every upgrade item = permission to buy it at Piero's (we grant it).

