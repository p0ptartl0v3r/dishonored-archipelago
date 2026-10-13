"""Lets these tests import the world's pure-Python modules without Archipelago.

worlds/dishonored/__init__.py imports Archipelago, so we register the folder as
the package "dishonored" without executing that file.
"""

import importlib.util
import sys
from pathlib import Path

WORLD_DIR = Path(__file__).resolve().parent.parent / "worlds" / "dishonored"

spec = importlib.util.spec_from_file_location(
    "dishonored", WORLD_DIR / "__init__.py", submodule_search_locations=[str(WORLD_DIR)]
)
sys.modules.setdefault("dishonored", importlib.util.module_from_spec(spec))
