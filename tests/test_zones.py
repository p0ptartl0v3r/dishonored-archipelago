import json
from pathlib import Path

from dishonored.client.zones import LEVEL_PREFIX_ZONE_KEYS, LEVEL_ZONE_KEYS, zone_key_for_level, zone_key_for_travel

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


def test_travel_prompts_lead_into_known_zones():
    from dishonored.client.zones import TRAVEL_PROMPTS, zone_key_for_travel

    names = {item["name"] for item in json.loads(ITEMS.read_text(encoding="utf-8"))}
    for option, level in TRAVEL_PROMPTS.items():
        assert "#" in option and " " not in option  # one word: "<action path>#<index>"
        assert zone_key_for_travel(option) in names, (option, level)
    bottle_street = "L_Distillery_Strt1Script.TheWorld.PersistentLevel.Main_Sequence.DisSeqAct_DialogScriptedChoice_1#0"
    assert zone_key_for_travel(bottle_street) == "Distillery District (HOC) Zone Key"
    assert zone_key_for_travel("L_Unknown.DisSeqAct_DialogScriptedChoice_0#0") is None


def test_galvanis_exits_depend_on_the_mission():
    # Both missions' Galvani offices use the L_Galvani1_Scripts level script.
    from dishonored.client.zones import zone_key_for_travel

    exit_door = "L_Galvani1_Scripts.TheWorld.PersistentLevel.Main_Sequence.DisSeqAct_DialogScriptedChoice_3#0"
    assert zone_key_for_travel(exit_door, "L_Galvani1_P") == "Distillery District (HOC) Zone Key"
    assert zone_key_for_travel(exit_door, "L_Galvani2_P") == "Distillery District (HoP) Zone Key"
    assert zone_key_for_travel(exit_door) is None


def test_the_royal_physicians_bridge_areas():
    assert zone_key_for_level("L_Bridge_Part1a_P") == "Southside Gate Zone Key"
    assert zone_key_for_level("L_Bridge_Part1b_P") == "Drawbridge Control Zone Key"
    assert zone_key_for_level("L_Bridge_Part1c_P") == "Midrow Substation Zone Key"
    assert zone_key_for_level("L_Bridge_Part2_P") == "North End Zone Key"


def test_boyle_mansion_doors():
    choice = "TheWorld.PersistentLevel.Main_Sequence.DisSeqAct_DialogScriptedChoice_"
    assert zone_key_for_travel(f"L_Boyle_Ext_P.{choice}4#0") == "Boyle Mansion Interior Zone Key"
    assert zone_key_for_travel(f"L_Boyle_Ext_P.{choice}0#0") == "Boyle Mansion Interior Zone Key"
    assert zone_key_for_travel(f"L_Boyle_Int_P.{choice}1#0") == "Lady Boyle's Last Party Exterior Zone Key"


def test_tower_doors():
    choice = "TheWorld.PersistentLevel.Main_Sequence.DisSeqAct_DialogScriptedChoice_"
    assert zone_key_for_travel(f"L_TowerRtrn_Yard_Script.{choice}5#0") == "Tower Interior Zone Key"
    # The rooftop is in the exterior level.
    assert zone_key_for_travel(f"L_TowerRtrn_Int_Script.{choice}5#0") == "Tower Exterior Zone Key"


def test_flooded_district_doors():
    choice = "TheWorld.PersistentLevel.Main_Sequence.DisSeqAct_DialogScriptedChoice_"
    # Taken without the key on 2026-10-29: the safety net sent the player to the menu instead.
    assert zone_key_for_travel(f"L_Flooded_FAssassins_P.{choice}1#0") == "Rudshore Gate Zone Key"
    assert zone_key_for_travel(f"L_Flooded_FStreets_P.{choice}4#0") == "Central Rudshore Zone Key"
    assert zone_key_for_level("L_Flooded_FIntro_P") is None
