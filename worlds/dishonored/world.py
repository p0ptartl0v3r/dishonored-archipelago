from __future__ import annotations

from collections.abc import Mapping
from typing import Any

from BaseClasses import CollectionState, Item, ItemClassification, Location, Region, Tutorial
from worlds.AutoWorld import WebWorld, World
from worlds.generic.Rules import set_rule

from . import game_data, requirements
from .options import BONE_CHARM_SLOTS_WITHOUT_DLC, STARTING_ZONE_KEYS, DishonoredOptions

DATA = game_data.load()


class DishonoredItem(Item):
    game = game_data.GAME_NAME


class DishonoredLocation(Location):
    game = game_data.GAME_NAME


class _PlayerState:
    """Adapts CollectionState to the StateView the requirement rules expect."""

    __slots__ = ("state", "player")

    def __init__(self, state: CollectionState, player: int) -> None:
        self.state = state
        self.player = player

    def count(self, item: str) -> int:
        return self.state.count(item, self.player)

    def count_from_list(self, items) -> int:
        return self.state.count_from_list(items, self.player)


def _items_used_by_logic() -> set[str]:
    used: set[str] = set()
    nodes = [l.requires for l in DATA.locations.values()] + [r.requires for r in DATA.regions.values()]
    for node in nodes:
        used |= requirements.referenced_items(node, DATA.item_categories)
    return used


# Anything a rule looks at has to be progression, or the generator won't count it
# (e.g. "|@Combat:2|" counts Progressive Sword, which items.json marks as only useful).
PROGRESSION_ITEMS = {i.name for i in DATA.items.values() if i.progression} | _items_used_by_logic()


class DishonoredWeb(WebWorld):
    theme = "stone"
    tutorials = [
        Tutorial(
            "Multiworld Setup Guide",
            "A guide to setting up Dishonored for Archipelago.",
            "English",
            "setup_en.md",
            "setup/en",
            ["p0ptartl0v3r"],
        )
    ]


class DishonoredWorld(World):
    """
    Dishonored is a stealth-action game set in the plague-ridden city of Dunwall.
    Powers, equipment, upgrades, bone charm slots and access to each area of the
    city are shuffled into the multiworld.
    """

    game = game_data.GAME_NAME
    web = DishonoredWeb()
    options_dataclass = DishonoredOptions
    options: DishonoredOptions

    item_name_to_id = DATA.item_name_to_id
    location_name_to_id = DATA.location_name_to_id
    item_name_groups = {cat: set(names) for cat, names in DATA.item_categories.items()}
    location_name_groups = {cat: set(names) for cat, names in DATA.location_categories.items()}
    origin_region_name = next(r.name for r in DATA.regions.values() if r.starting)

    def _rule(self, node: requirements.Node):
        rule = requirements.compile_rule(node, DATA.item_categories, DATA.item_counts)
        if rule is None:
            return None
        player = self.player
        return lambda state: rule(_PlayerState(state, player))

    def create_regions(self) -> None:
        regions = {name: Region(name, self.player, self.multiworld) for name in DATA.regions}
        self.multiworld.regions += regions.values()

        for data in DATA.regions.values():
            for target in data.connects_to:
                # A region's "requires" is what it takes to enter it.
                regions[data.name].connect(regions[target], f"{data.name} -> {target}", self._rule(DATA.regions[target].requires))

        for data in DATA.locations.values():
            region = regions[data.region]
            location = DishonoredLocation(self.player, data.name, data.id, region)
            region.locations.append(location)
            if data.victory:
                location.place_locked_item(
                    DishonoredItem(game_data.VICTORY_ITEM, ItemClassification.progression, None, self.player)
                )
            elif data.place_item:
                location.place_locked_item(self.create_item(data.place_item))

        self.multiworld.completion_condition[self.player] = lambda state: state.has(
            game_data.VICTORY_ITEM, self.player
        )

    def set_rules(self) -> None:
        for data in DATA.locations.values():
            rule = self._rule(data.requires)
            if rule is not None:
                set_rule(self.get_location(data.name), rule)

    def create_item(self, name: str) -> DishonoredItem:
        data = DATA.items[name]
        if name in PROGRESSION_ITEMS:
            classification = ItemClassification.progression
        elif data.useful:
            classification = ItemClassification.useful
        else:
            classification = ItemClassification.filler
        return DishonoredItem(name, classification, data.id, self.player)

    def create_items(self) -> None:
        pool: list[DishonoredItem] = []
        locked = [l.place_item for l in DATA.locations.values() if l.place_item]
        starting = STARTING_ZONE_KEYS[self.options.starting_zone_keys.value]
        for name in starting:
            self.multiworld.push_precollected(self.create_item(name))
        for data in DATA.items.values():
            count = data.count - locked.count(data.name) - starting.count(data.name)
            if data.name == "Bone Charm Slot" and not self.options.void_walkers_arsenal:
                count = min(count, BONE_CHARM_SLOTS_WITHOUT_DLC)  # the game caps slots at 6 without the DLC
            pool += [self.create_item(data.name) for _ in range(count)]

        open_locations = sum(1 for l in DATA.locations.values() if not l.victory and not l.place_item)
        if len(pool) > open_locations:
            raise RuntimeError(f"{self.game}: {len(pool)} items but only {open_locations} locations")
        pool += [self.create_filler() for _ in range(open_locations - len(pool))]
        self.multiworld.itempool += pool

    def get_filler_item_name(self) -> str:
        return game_data.FILLER_ITEM

    def fill_slot_data(self) -> Mapping[str, Any]:
        # Everything the client/game needs to know about this seed goes here.
        return {
            "world_version": 1,
            **self.options.as_dict("death_link", "void_walkers_arsenal", "starting_zone_keys"),
        }
