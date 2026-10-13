"""Package worlds/dishonored into dishonored.apworld (a zip Archipelago can install).

Usage: python tools/build_apworld.py [--out DIR]
"""

from __future__ import annotations

import argparse
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WORLD = ROOT / "worlds" / "dishonored"
SKIP_DIRS = {"__pycache__", "test"}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=ROOT / "dist")
    args = parser.parse_args()

    args.out.mkdir(parents=True, exist_ok=True)
    target = args.out / "dishonored.apworld"
    with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED) as zf:
        for path in sorted(WORLD.rglob("*")):
            rel = path.relative_to(WORLD)
            if path.is_dir() or SKIP_DIRS & set(rel.parts) or path.suffix == ".pyc":
                continue
            # Archipelago expects everything inside a top-level folder named after the world.
            zf.write(path, Path("dishonored") / rel)
    print(f"wrote {target}")


if __name__ == "__main__":
    main()
