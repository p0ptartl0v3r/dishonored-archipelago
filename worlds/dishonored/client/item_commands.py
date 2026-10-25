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

from collections.abc import Callable, Mapping

CommandBuilder = Callable[[int], list[str]]


MAX_POWER_LEVEL = 2  # every power in the base game has two levels


def _power(internal_name: str) -> CommandBuilder:
    # Extra copies (e.g. from !getitem) would ask for level 3+; stay at the maximum.
    return lambda level: [f"console AddPower {internal_name} {min(level, MAX_POWER_LEVEL)}"]


def _upgrades(*tiers: str) -> CommandBuilder:
    # Copy N of a progressive upgrade gives tiers 1..N; the DLL skips tiers already owned.
    return lambda level: [f"upgrade {name}" for name in tiers[:level]]


# Item -> power name as listed by the in-game AddPower command (confirmed in-game).
POWERS: dict[str, str] = {
    "Progressive Blink": "Blink",
    "Progressive Dark Vision": "DarkVision",
    "Progressive Devouring Swarm": "DevouringSwarm",
    "Progressive Possession": "Possess",
    "Progressive Bend Time": "BendTime",
    "Progressive Windblast": "WindBlast",
    "Progressive Vitality": "Vitality",
    "Progressive Agility": "Celerity",  # Agility's internal name
    "Progressive Blood Thirsty": "BloodThirsty",
    "Progressive Shadow Kill": "ShadowKill",
}

ITEM_COMMANDS: dict[str, CommandBuilder] = {
    **{item: _power(power) for item, power in POWERS.items()},
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
# total at or below N; the DLL then sets the base capacity (bone_charm_base) so the total
# is exactly N, from 0 slots with no items up to 10.
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


# Equipment items allow using an ammo type (the DLL's EeDisAmmoType names): copy N of a
# progressive item allows the first N types. Until then the DLL keeps that type's count,
# and a loaded magazine of it, at 0, and refunds shop purchases of it.
AMMO: dict[str, tuple[str, ...]] = {
    "Progressive Bullets": ("Bullet", "Bullet_Explosive"),
    "Progressive Crossbow Bolts": ("Arrow", "Arrow_Flare"),  # Arrow_Flare = incendiary bolts
    "Progressive Sleep Darts": ("Arrow_Sleep",),
    "Progressive Grenades": ("Grenade", "StickyGrenade"),
    "Spring Razor": ("SpringRazor",),
}

# Items that allow a collectable (abstract) item; held at 0 until received. The Rewire
# Tool has shown up under both names (inventory list and pickup).
ABSTRACT_ITEMS: dict[str, tuple[str, ...]] = {
    "Rewire Tool": ("RewireTool", "RewireTool_AbsItm"),
}

# Items whose effect comes (at least partly) from the limits rather than a command.
APPLIED_BY_LIMITS: set[str] = set(AMMO) | set(ABSTRACT_ITEMS) | {"Bone Charm Slot"}

BASE_BONE_CHARM_SLOTS = 3  # Corvo's slots without any capacity upgrade


def bone_charm_base(slots: int) -> int:
    """Base capacity the DLL should set so that base + granted upgrades = ``slots``
    (the Manual's one slot per item, starting from none)."""
    granted = max((total for total, _ in BONE_CHARM_CAPACITY if total <= slots), default=BASE_BONE_CHARM_SLOTS)
    return slots - (granted - BASE_BONE_CHARM_SLOTS)


def limits(received: Mapping[str, int]) -> list[str]:
    """DLL "limit" requests: how far each randomized power and upgrade may go, given how
    many copies of each item have been received. The DLL blocks and removes anything beyond."""
    lines = [
        f"limit power {power} {min(received.get(item, 0), MAX_POWER_LEVEL)}" for item, power in POWERS.items()
    ]
    for item, builder in ITEM_COMMANDS.items():
        if item in POWERS:
            continue
        # Every upgrade the item can ever give is randomized; those for copies so far are allowed.
        every = [r.removeprefix("upgrade ") for r in builder(99) if r.startswith("upgrade ")]
        allowed = {r.removeprefix("upgrade ") for r in builder(received.get(item, 0))}
        lines += [f"limit upgrade {name} {int(name in allowed)}" for name in every]
    lines.append(f"limit bonecharms base {bone_charm_base(received.get('Bone Charm Slot', 0))}")
    for item, types in AMMO.items():
        lines += [f"limit ammo {ammo} {int(i < received.get(item, 0))}" for i, ammo in enumerate(types)]
    for item, names in ABSTRACT_ITEMS.items():
        lines += [f"limit item {name} {int(received.get(item, 0) > 0)}" for name in names]
    return lines


def console_commands(requests: list[str]) -> list[str]:
    """The console commands among ``requests``, i.e. what a player could type themselves."""
    return [r.removeprefix("console ") for r in requests if r.startswith("console ")]
