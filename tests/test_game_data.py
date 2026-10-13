import json
from pathlib import Path

from dishonored import game_data


def test_data_loads_and_validates():
    data = game_data.load()
    assert len(data.items) > 0
    assert sum(1 for l in data.locations.values() if l.victory) == 1


def test_ids_are_unique():
    data = game_data.load()
    item_ids = list(data.item_name_to_id.values())
    location_ids = list(data.location_name_to_id.values())
    assert len(item_ids) == len(set(item_ids))
    assert len(location_ids) == len(set(location_ids))


def test_ids_are_append_only():
    """ids.json is append-only: an existing entry must never get a different number."""
    ids = json.loads((Path(game_data.__file__).parent / "data" / "ids.json").read_text("utf-8"))
    for kind in ("items", "locations"):
        values = list(ids[kind].values())
        assert values == sorted(values), f"{kind}: IDs should only ever be appended"


def test_enough_locations_for_items():
    data = game_data.load()
    items = sum(i.count for i in data.items.values())
    locations = sum(1 for l in data.locations.values() if not l.victory)
    assert items <= locations

