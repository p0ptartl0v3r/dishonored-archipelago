import json
from pathlib import Path

from dishonored.client.zones import LEVEL_PREFIX_ZONE_KEYS, LEVEL_ZONE_KEYS, zone_key_for_level

ITEMS = Path(__file__).resolve().parent.parent / "worlds" / "dishonored" / "data" / "items.json"


def test_levels_from_the_first_two_missions():
    assert zone_key_for_level("L_Prison_P") == "Dunwall Prison Zone Key"
    assert zone_key_for_level("L_Pub_Dusk_P") == "Hound Pits Pub Zone Key"
    assert zone_key_for_level("L_Pub_Assault_P") == "Hound Pits Pub (The Loyalists) Zone Key"
    assert zone_key_for_level("L_Ovrsr_Back_P") == "Backyard Zone Key"


def test_sublevels_and_unknown_levels_are_not_zones():
    assert zone_key_for_level("L_Pub_FromPrison_Script") is None
    assert zone_key_for_level("l_tower_p") is None  # the prologue has no key
    assert zone_key_for_level("Dishonored_MainMenu") is None


def test_every_key_is_a_real_item():
    names = {i["name"] for i in json.loads(ITEMS.read_text("utf-8"))}
    assert set(LEVEL_ZONE_KEYS.values()) | {key for _, key in LEVEL_PREFIX_ZONE_KEYS} <= names
