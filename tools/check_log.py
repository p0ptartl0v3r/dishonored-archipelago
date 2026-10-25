"""Which locations of a mission did a play session's events complete?

Reads the DLL's archipelago_native.log and matches its events against each location's
`detect` patterns (the client's own matching), without Archipelago or the game running.

    python tools/check_log.py "High Overseer Campbell"
    python tools/check_log.py "House of Pleasure" --log path/to/archipelago_native.log --since 03:40:00
"""

from __future__ import annotations

import argparse
import importlib.util
import os
import re
import sys
from pathlib import Path

WORLD = Path(__file__).resolve().parent.parent / "worlds" / "dishonored"
# worlds/dishonored/__init__.py imports Archipelago; register the folder without running it.
spec = importlib.util.spec_from_file_location("dishonored", WORLD / "__init__.py", submodule_search_locations=[str(WORLD)])
sys.modules.setdefault("dishonored", importlib.util.module_from_spec(spec))

from dishonored.client.game_interface import event_matches  # noqa: E402
from dishonored.game_data import load  # noqa: E402

LOGS = [
    Path(r"C:\Program Files (x86)\Steam\steamapps\common\Dishonored\Binaries\Win32\archipelago_native.log"),
    Path(os.environ.get("LOCALAPPDATA", "")) / "DishonoredArchipelago" / "archipelago_native.log",
]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("mission", help='location category, e.g. "High Overseer Campbell" or "Intro"')
    parser.add_argument("--log", type=Path, help="the DLL's log (default: the game folder's, then %%LOCALAPPDATA%%)")
    parser.add_argument("--since", default="", help="only events at or after this time (HH:MM:SS)")
    args = parser.parse_args()
    log = args.log or next((p for p in LOGS if p.is_file()), None)
    if log is None:
        sys.exit("archipelago_native.log not found; pass --log")

    events = []
    for line in log.read_text(encoding="utf-8", errors="replace").splitlines():
        match = re.match(r"\[(\S+)\] event: (.*)", line)
        # Pickups destroyed while a save loads were collected before that save, not now.
        if match and match.group(1) >= args.since and not match.group(2).endswith("(load)"):
            events.append((match.group(1), match.group(2)))

    found = missing = 0
    for location in load().locations.values():
        if args.mission not in location.categories:
            continue
        hit = next((time for time, event in events for p in location.detect if event_matches(p, event)), None)
        if hit:
            found += 1
            print(f"OK       {hit}  {location.name}")
        else:
            missing += 1
            print(f"{'MISSING' if location.detect else 'NO DETECT'}  {'':12}  {location.name}")
    print(f"{found} found, {missing} not")


if __name__ == "__main__":
    main()
