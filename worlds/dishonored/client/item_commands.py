"""Requests that grant each Archipelago item in-game.

Each request is a line for the in-game DLL (native/src/pipe_server.hpp):
``console <command>`` runs a console command, ``upgrade <name>`` gives one of
Piero's upgrades by its DisTweaks_Upgrade object name. The manual backend shows
the console commands for the player to type.

Each entry maps an item name to a function of ``level``: how many copies of that
item the player has received so far, including this one (1 for the first
Progressive Blink, 2 for the second). Requests must describe the absolute state
for that level, because every item is applied again after each save load.

Names come from the game's class/enum names (see research/prior-art.md).
Anything listed in UNVERIFIED hasn't been confirmed in-game yet.
No Archipelago imports here, so tests can load it standalone.
"""

from __future__ import annotations

from collections.abc import Callable

CommandBuilder = Callable[[int], list[str]]


MAX_POWER_LEVEL = 2  # every power in the base game has two levels


def _power(internal_name: str) -> CommandBuilder:
    # Extra copies (e.g. from !getitem) would ask for level 3+; stay at the maximum.
    return lambda level: [f"console AddPower {internal_name} {min(level, MAX_POWER_LEVEL)}"]


def _upgrades(*tiers: str) -> CommandBuilder:
    # Copy N of a progressive upgrade gives tiers 1..N; the DLL skips tiers already owned.
    return lambda level: [f"upgrade {name}" for name in tiers[:level]]


ITEM_COMMANDS: dict[str, CommandBuilder] = {
    # Names as listed by the in-game AddPower command (confirmed in-game).
    "Progressive Blink": _power("Blink"),
    "Progressive Dark Vision": _power("DarkVision"),
    "Progressive Devouring Swarm": _power("DevouringSwarm"),
    "Progressive Possession": _power("Possess"),
    "Progressive Bend Time": _power("BendTime"),
    "Progressive Windblast": _power("WindBlast"),
    "Progressive Vitality": _power("Vitality"),
    "Progressive Agility": _power("Celerity"),  # Agility's internal name
    "Progressive Blood Thirsty": _power("BloodThirsty"),
    "Progressive Shadow Kill": _power("ShadowKill"),
    # Piero's upgrades: DisTweaks_Upgrade object names from the DLL's "dump" (confirmed
    # in-game), with the shop's display name in the comment.
    "Corvo's Pistol": _upgrades("Twk_Upgrade_Pistol"),
    "Progressive Pistol Accuracy": _upgrades("Twk_Upgrade_Pistol_Accuracy1", "Twk_Upgrade_Pistol_Accuracy2"),
    "Progressive Pistol Magazine": _upgrades(
        "Twk_Upgrade_PistolMagazine1", "Twk_Upgrade_PistolMagazine2", "Twk_Upgrade_PistolMagazine3"
    ),
    "Progressive Pistol Reload Speed": _upgrades("Twk_Upgrade_PistolReloadSpeed1", "Twk_Upgrade_PistolReloadSpeed2"),
    "Progressive Bullet Capacity": _upgrades("Twk_Upgrade_AmmoPouch2", "Twk_Upgrade_AmmoPouch4"),
    "Progressive Bolt Capacity": _upgrades("Twk_Upgrade_QuiverCapacity2", "Twk_Upgrade_QuiverCapacity4"),
    "Crossbow Accuracy": _upgrades("Twk_Upgrade_Crossbow_Accuracy"),
    "Crossbow Range": _upgrades("Twk_Upgrade_Crossbow_Range"),
    "Crossbow Reload": _upgrades("Twk_Upgrade_Crossbow_ReloadSpeed"),
    "Grenade Capacity": _upgrades("Twk_Upgrade_GrenadeBelt"),
    "Spring Razor Capacity": _upgrades("Twk_Upgrade_SpringRazorPouch"),
    "Spring Razor Radius": _upgrades("Twk_Upgrade_SpringRazor"),
    "Progressive Mask Optics": _upgrades("Twk_Upgrade_Spyglass1", "Twk_Upgrade_Spyglass2"),
    "Improved Armor": _upgrades("Twk_Upgrade_Armor"),
    "Progressive Boot Stealth": _upgrades("Twk_Upgrade_SilentRunning", "Twk_Upgrade_SilentSprinting"),
}

# The second copy of these is an upgrade; the first (being allowed to use the
# equipment at all) isn't automated yet, so it's shown to the player.
_SECOND_TIER_UPGRADES = {
    "Progressive Sword": "Twk_Upgrade_Sword",  # Sword Crossing Upgrade
    "Progressive Sleep Darts": "Twk_Upgrade_InstantSleepArrow",  # Combat Sleep Darts
    "Progressive Bullets": "Twk_Upgrade_Gun_Explosive",  # Pistol Explosive Shot Upgrade
    "Progressive Grenades": "Twk_Upgrade_StickyGrenade",  # Sticky Grenade Upgrade
}
for _item, _name in _SECOND_TIER_UPGRADES.items():
    ITEM_COMMANDS[_item] = lambda level, name=_name: [f"upgrade {name}"] if level >= 2 else []

# Bone charm capacity: Corvo starts with 3 slots; Piero's two upgrades add 1 and 2,
# and each DLC "Mystical ... Artifact" statue adds 1 (10 in total, one per Manual item).
# Each entry is (slots once owned, upgrade). Copy N grants every upgrade that keeps the
# total at or below N, so the slot count matches N except: 1-3 copies leave the base 3
# (fewer needs the DLL to cap slots; not done yet), and 5 copies give 4.
BONE_CHARM_CAPACITY = [
    (4, "Twk_Upgrade_BoneCharms"),  # Bone Charm Capacity Upgrade 1
    (6, "Twk_Upgrade_BoneCharms2"),  # Bone Charm Capacity Upgrade 2 (requires 1)
    (7, "Twk_Upgrade_StatueFish"),
    (8, "Twk_Upgrade_StatueWhale"),
    (9, "Twk_Upgrade_StatueWolfhound"),
    (10, "Twk_Upgrade_StatueRat"),
]
ITEM_COMMANDS["Bone Charm Slot"] = lambda level: [
    f"upgrade {name}" for slots, name in BONE_CHARM_CAPACITY if slots <= level
]

# Not mapped yet: Progressive Crossbow Bolts 2 (incendiary bolts) is a blueprint
# (BP_FlareBolt_AbsItm), not an upgrade.

# Items whose requests haven't been confirmed in-game. The client shows these to
# the player instead of sending them, because a wrong name silently does nothing.
# Blink is confirmed working end to end; the other powers' names come from the
# in-game AddPower command's own list.
UNVERIFIED: set[str] = set()


def commands_for(item_name: str, level: int) -> list[str]:
    """DLL requests for receiving copy number ``level`` of an item ([] if unknown)."""
    builder = ITEM_COMMANDS.get(item_name)
    return builder(level) if builder else []


def console_commands(requests: list[str]) -> list[str]:
    """The console commands among ``requests``, i.e. what a player could type themselves."""
    return [r.removeprefix("console ") for r in requests if r.startswith("console ")]
