"""Console commands that grant each Archipelago item in-game.

Used in two ways: the manual backend prints these for the player to type into
the developer console, and a future native backend will run them directly.

Each entry maps an item name to a function of ``level``: how many copies of that
item the player has received so far, including this one (1 for the first
Progressive Blink, 2 for the second).

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
    return lambda level: [f"AddPower {internal_name} {min(level, MAX_POWER_LEVEL)}"]


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
}

# Commands whose names haven't been confirmed in-game. The client shows these to
# the player instead of sending them, because a wrong name silently does nothing.
# Blink is confirmed working end to end; the other powers' names come from the
# in-game AddPower command's own list.
UNVERIFIED: set[str] = set()


def commands_for(item_name: str, level: int) -> list[str]:
    """Console commands for receiving copy number ``level`` of an item ([] if unknown)."""
    builder = ITEM_COMMANDS.get(item_name)
    return builder(level) if builder else []
