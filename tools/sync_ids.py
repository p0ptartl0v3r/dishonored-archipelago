"""Give every item and location in the data files a permanent numeric ID.

Archipelago identifies items and locations by number, and the game-side code
will hard-code those numbers. So IDs must never change once assigned: this
script only *appends* new names to worlds/dishonored/data/ids.json. Renaming
something in items.json/locations.json therefore creates a new ID; if you
really mean a rename, edit the key in ids.json by hand instead.

Usage:
    python tools/sync_ids.py          # add IDs for new names
    python tools/sync_ids.py --check  # exit 1 if anything is missing (CI)
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

DATA = Path(__file__).resolve().parent.parent / "worlds" / "dishonored" / "data"
ID_BASE = 0xD15_0000  # arbitrary; IDs only have to be unique within this game
FILLER_ITEM = "Whalebone"  # keep in sync with game_data.FILLER_ITEM


def wanted_names() -> tuple[list[str], list[str]]:
    items = [i["name"] for i in json.loads((DATA / "items.json").read_text("utf-8"))]
    items.append(FILLER_ITEM)
    locations = [
        l["name"]
        for l in json.loads((DATA / "locations.json").read_text("utf-8"))
        if not l.get("victory")  # the victory location is an event, it has no ID
    ]
    return items, locations


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check", action="store_true", help="only report, do not write")
    args = parser.parse_args()

    ids_path = DATA / "ids.json"
    ids = json.loads(ids_path.read_text("utf-8")) if ids_path.exists() else {"items": {}, "locations": {}}
    items, locations = wanted_names()

    added: list[str] = []
    unused: list[str] = []
    for kind, names in (("items", items), ("locations", locations)):
        table: dict[str, int] = ids[kind]
        next_id = max(table.values(), default=ID_BASE) + 1
        for name in names:
            if name not in table:
                table[name] = next_id
                next_id += 1
                added.append(f"{kind[:-1]} {name!r} -> {table[name]}")
        unused += [f"{kind[:-1]} {name!r}" for name in table if name not in names]

    for line in added:
        print(f"new: {line}")
    for line in unused:
        # Kept on purpose so the number is never reused for something else.
        print(f"unused (kept reserved): {line}")

    if args.check:
        if added:
            print("ids.json is out of date; run: python tools/sync_ids.py", file=sys.stderr)
            return 1
        return 0

    if added or not ids_path.exists():
        ids_path.write_text(json.dumps(ids, indent=2, ensure_ascii=False) + "\n", "utf-8")
        print(f"wrote {ids_path}")
    else:
        print("ids.json already up to date")
    return 0


if __name__ == "__main__":
    sys.exit(main())
