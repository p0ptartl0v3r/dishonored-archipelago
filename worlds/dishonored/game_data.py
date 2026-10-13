"""Loads the JSON data files and the locked ID table.

No Archipelago imports here either, so tools and tests can use it directly.
"""

from __future__ import annotations

import json
import pkgutil
from dataclasses import dataclass, field
from functools import cache
from typing import Any

from . import requirements

GAME_NAME = "Dishonored"
FILLER_ITEM = "Whalebone"
VICTORY_ITEM = "Victory"


def _load_json(name: str) -> Any:
    # pkgutil works both from a source checkout and from inside a zipped .apworld.
    raw = pkgutil.get_data(__name__.rpartition(".")[0], f"data/{name}")
    if raw is None:
        raise FileNotFoundError(name)
    return json.loads(raw.decode("utf-8"))


@dataclass(frozen=True)
class ItemData:
    name: str
    id: int
    count: int
    categories: tuple[str, ...]
    progression: bool
    useful: bool


@dataclass(frozen=True)
class LocationData:
    name: str
    id: int | None  # None for the victory event, which the server never sees
    region: str
    categories: tuple[str, ...]
    requires: requirements.Node
    victory: bool
    place_item: str | None


@dataclass(frozen=True)
class RegionData:
    name: str
    starting: bool
    connects_to: tuple[str, ...]
    requires: requirements.Node


@dataclass
class GameData:
    items: dict[str, ItemData] = field(default_factory=dict)
    locations: dict[str, LocationData] = field(default_factory=dict)
    regions: dict[str, RegionData] = field(default_factory=dict)
    item_categories: dict[str, list[str]] = field(default_factory=dict)
    location_categories: dict[str, list[str]] = field(default_factory=dict)

    @property
    def item_name_to_id(self) -> dict[str, int]:
        return {i.name: i.id for i in self.items.values()}

    @property
    def location_name_to_id(self) -> dict[str, int]:
        return {l.name: l.id for l in self.locations.values() if l.id is not None}

    @property
    def item_counts(self) -> dict[str, int]:
        return {i.name: i.count for i in self.items.values()}


@cache
def load() -> GameData:
    ids = _load_json("ids.json")
    data = GameData()

    for raw in _load_json("items.json"):
        name = raw["name"]
        if name not in ids["items"]:
            raise KeyError(f"Item {name!r} has no ID; run tools/sync_ids.py")
        item = ItemData(
            name=name,
            id=ids["items"][name],
            count=int(raw.get("count", 1)),
            categories=tuple(raw.get("category", [])),
            progression=bool(raw.get("progression")),
            useful=bool(raw.get("useful")),
        )
        data.items[name] = item
        for cat in item.categories:
            data.item_categories.setdefault(cat, []).append(name)

    if FILLER_ITEM not in ids["items"]:
        raise KeyError(f"Filler item {FILLER_ITEM!r} has no ID; run tools/sync_ids.py")
    data.items[FILLER_ITEM] = ItemData(FILLER_ITEM, ids["items"][FILLER_ITEM], 0, (), False, False)

    for raw in _load_json("locations.json"):
        name = raw["name"]
        victory = bool(raw.get("victory"))
        if not victory and name not in ids["locations"]:
            raise KeyError(f"Location {name!r} has no ID; run tools/sync_ids.py")
        place = raw.get("place_item")
        if isinstance(place, list):
            if len(place) != 1:
                raise ValueError(f"{name}: only a single place_item is supported")
            place = place[0]
        loc = LocationData(
            name=name,
            id=None if victory else ids["locations"][name],
            region=raw["region"],
            categories=tuple(raw.get("category", [])),
            requires=requirements.parse(raw.get("requires")),
            victory=victory,
            place_item=place,
        )
        data.locations[name] = loc
        for cat in loc.categories:
            data.location_categories.setdefault(cat, []).append(name)

    for name, raw in _load_json("regions.json").items():
        data.regions[name] = RegionData(
            name=name,
            starting=bool(raw.get("starting")),
            connects_to=tuple(raw.get("connects_to", [])),
            requires=requirements.parse(raw.get("requires")),
        )

    _validate(data)
    return data


def _validate(data: GameData) -> None:
    if sum(r.starting for r in data.regions.values()) != 1:
        raise ValueError("Exactly one region must be marked starting")
    if sum(l.victory for l in data.locations.values()) != 1:
        raise ValueError("Exactly one location must be marked victory")
    for region in data.regions.values():
        for target in region.connects_to:
            if target not in data.regions:
                raise ValueError(f"{region.name} connects to unknown region {target!r}")
    for loc in data.locations.values():
        if loc.region not in data.regions:
            raise ValueError(f"{loc.name} is in unknown region {loc.region!r}")
        if loc.place_item and loc.place_item not in data.items:
            raise ValueError(f"{loc.name} places unknown item {loc.place_item!r}")
    nodes = [l.requires for l in data.locations.values()] + [r.requires for r in data.regions.values()]
    for node in nodes:
        # Raises on unknown items/categories.
        requirements.compile_rule(node, data.item_categories, data.item_counts)
