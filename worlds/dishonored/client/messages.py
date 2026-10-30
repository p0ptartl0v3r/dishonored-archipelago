"""Coloured lines for the in-game message overlay.

The DLL draws a message in segments: "{RRGGBB}" switches the colour of the text that
follows (default white). The colours follow Archipelago's text client: progression items
purple, useful blue, traps salmon, other items cyan, your own name magenta, other players
yellow, locations green. No Archipelago imports here, so tests can load it standalone.
"""

from __future__ import annotations

import re

WHITE = "FFFFFF"
SELF = "EE00EE"
OTHER_PLAYER = "FAFAD2"
LOCATION = "00FF7F"

# NetworkItem flags
PROGRESSION, USEFUL, TRAP = 0b001, 0b010, 0b100


def item_color(flags: int) -> str:
    if flags & PROGRESSION:
        return "AF99EF"
    if flags & USEFUL:
        return "6D8BE8"
    if flags & TRAP:
        return "FA8072"
    return "00EEEE"


def colored(text: str, color: str) -> str:
    # "{" could be read as a colour marker, so it never appears inside a segment; the
    # overlay shows one line, so whitespace runs (newlines included) become one space.
    return "{" + color + "}" + re.sub(r"\s+", " ", text.replace("{", "("))


def received_line(item: str, flags: int, sender: str, from_self: bool, location: str) -> str:
    """'Received <item> from <player> (<location>)', or 'Found <item> (<location>)' for your own."""
    if from_self:
        return colored("Found ", WHITE) + colored(item, item_color(flags)) + colored(" (", WHITE) + \
            colored(location, LOCATION) + colored(")", WHITE)
    return colored("Received ", WHITE) + colored(item, item_color(flags)) + colored(" from ", WHITE) + \
        colored(sender, OTHER_PLAYER) + colored(" (", WHITE) + colored(location, LOCATION) + colored(")", WHITE)


def chat_line(speaker: str, from_self: bool, text: str) -> str:
    """'<speaker>: <text>' for Archipelago chat; the server console speaks as "Server"."""
    return colored(speaker, SELF if from_self else OTHER_PLAYER) + colored(": " + text, WHITE)


WARNING = "FA8072"


def _the(zone_key: str) -> str:
    """The article before a key's name: none when the name starts with "The" already."""
    return "" if zone_key.lower().startswith("the ") else "the "


def missing_key_line(zone_key: str) -> str:
    """Shown when the player is in a zone without its key, before being sent to the main menu."""
    return colored(f"You don't have {_the(zone_key)}", WARNING) + colored(zone_key, item_color(PROGRESSION)) + \
        colored(" yet: returning to the main menu", WARNING)


def sent_to_menu_notice(zone_key: str) -> str:
    """Shown at the top of the main menu after being sent there for a missing zone key."""
    return colored(f"Sent back to the main menu: you don't have {_the(zone_key)}", WARNING) + \
        colored(zone_key, item_color(PROGRESSION)) + colored(" yet", WARNING)


def requires_key_line(zone_key: str) -> str:
    """Shown when a travel prompt was refused because its zone key hasn't been received."""
    return colored(f"Requires {_the(zone_key)}", WARNING) + colored(zone_key, item_color(PROGRESSION))


def sent_line(item: str, flags: int, receiver: str, location: str) -> str:
    """'Sent <item> to <player> (<location>)' for an item found for someone else."""
    return colored("Sent ", WHITE) + colored(item, item_color(flags)) + colored(" to ", WHITE) + \
        colored(receiver, OTHER_PLAYER) + colored(" (", WHITE) + colored(location, LOCATION) + colored(")", WHITE)
