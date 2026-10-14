import json
from pathlib import Path

from dishonored.client.item_commands import ITEM_COMMANDS, UNVERIFIED, commands_for

ITEMS = Path(__file__).resolve().parent.parent / "worlds" / "dishonored" / "data" / "items.json"


def test_progressive_power_levels():
    assert commands_for("Progressive Blink", 1) == ["AddPower Blink 1"]
    assert commands_for("Progressive Blink", 2) == ["AddPower Blink 2"]


def test_unknown_item_has_no_commands():
    assert commands_for("Whalebone", 1) == []


def test_mapped_names_are_real_items():
    names = {i["name"] for i in json.loads(ITEMS.read_text("utf-8"))}
    assert set(ITEM_COMMANDS) <= names
    assert UNVERIFIED <= set(ITEM_COMMANDS)
