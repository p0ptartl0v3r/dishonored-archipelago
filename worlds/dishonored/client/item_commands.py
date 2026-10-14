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


def _power(internal_name: str) -> CommandBuilder:
    return lambda level: [f"AddPower {internal_name} {level}"]


ITEM_COMMANDS: dict[str, CommandBuilder] = {
    # Active powers (internal names from the EDisPowerWheelItem enum)
    "Progressive Blink": _power("Blink"),
    "Progressive Dark Vision": _power("DarkVision"),
    "Progressive Devouring Swarm": _power("DevouringSwarm"),
    "Progressive Possession": _power("Possession"),
    "Progressive Bend Time": _power("BendTime"),
    "Progressive Windblast": _power("WindBlast"),
    # Passive powers: names are guesses
    "Progressive Vitality": _power("Vitality"),
    "Progressive Agility": _power("Agility"),
    "Progressive Blood Thirsty": _power("BloodThirsty"),
    "Progressive Shadow Kill": _power("ShadowKill"),
}

# Commands not yet confirmed to work in-game with these exact arguments.
# Remove an entry once you have tried it (and record it in research/notes.md).
UNVERIFIED: set[str] = {
    "Progressive Dark Vision",
    "Progressive Devouring Swarm",
    "Progressive Possession",
    "Progressive Bend Time",
    "Progressive Windblast",
    "Progressive Vitality",
    "Progressive Agility",
    "Progressive Blood Thirsty",
    "Progressive Shadow Kill",
}


def commands_for(item_name: str, level: int) -> list[str]:
    """Console commands for receiving copy number ``level`` of an item ([] if unknown)."""
    builder = ITEM_COMMANDS.get(item_name)
    return builder(level) if builder else []
