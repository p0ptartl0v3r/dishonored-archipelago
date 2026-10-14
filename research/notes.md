# Research notes

## Game build
- Store / version:
- `Dishonored.exe` size / hash:

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
| `AddPower <Name> <Level>` | **Works.** Active power names from the SDK: `Blink`, `DarkVision`, `DevouringSwarm`, `Possession`, `BendTime`, `WindBlast`. Passive names unknown. Track per power in `client/item_commands.py` |
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

## Memory (Cheat Engine)

| Value | Module + pointer path | Type | Notes |
|---|---|---|---|
| Current map name | | | |
| Loading flag | | | |
| Rune count | | | |
