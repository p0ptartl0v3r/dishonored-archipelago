import json
from pathlib import Path

from dishonored.client.item_commands import ITEM_COMMANDS, UNVERIFIED, _upgrades, commands_for, console_commands

ITEMS = Path(__file__).resolve().parent.parent / "worlds" / "dishonored" / "data" / "items.json"


def test_progressive_power_levels():
    assert commands_for("Progressive Blink", 1) == ["console AddPower Blink 1"]
    assert commands_for("Progressive Blink", 2) == ["console AddPower Blink 2"]
    assert commands_for("Progressive Blink", 5) == ["console AddPower Blink 2"]  # capped at the max level


def test_unknown_item_has_no_commands():
    assert commands_for("Whalebone", 1) == []


def test_mapped_names_are_real_items():
    names = {i["name"] for i in json.loads(ITEMS.read_text("utf-8"))}
    assert set(ITEM_COMMANDS) <= names
    assert UNVERIFIED <= set(ITEM_COMMANDS)


def test_internal_names_that_differ_from_item_names():
    assert commands_for("Progressive Possession", 1) == ["console AddPower Possess 1"]
    assert commands_for("Progressive Agility", 2) == ["console AddPower Celerity 2"]


def test_progressive_upgrade_grants_every_tier_so_far():
    accuracy = _upgrades("Accuracy1", "Accuracy2")
    assert accuracy(1) == ["upgrade Accuracy1"]
    assert accuracy(2) == ["upgrade Accuracy1", "upgrade Accuracy2"]
    assert accuracy(3) == ["upgrade Accuracy1", "upgrade Accuracy2"]


def test_only_console_requests_are_shown_to_type():
    assert console_commands(["console AddPower Blink 1", "upgrade Accuracy1"]) == ["AddPower Blink 1"]


def test_second_copy_of_equipment_is_its_upgrade():
    assert commands_for("Progressive Sword", 1) == []  # using the sword at all isn't automated yet
    assert commands_for("Progressive Sword", 2) == ["upgrade Twk_Upgrade_Sword"]
    assert commands_for("Progressive Grenades", 2) == ["upgrade Twk_Upgrade_StickyGrenade"]


def test_every_piero_upgrade_tier_is_mapped():
    assert commands_for("Progressive Pistol Magazine", 3) == [
        "upgrade Twk_Upgrade_PistolMagazine1",
        "upgrade Twk_Upgrade_PistolMagazine2",
        "upgrade Twk_Upgrade_PistolMagazine3",
    ]
    items = {i["name"]: int(i.get("count", 1)) for i in json.loads(ITEMS.read_text("utf-8"))}
    for name in ("Progressive Pistol Accuracy", "Progressive Bullet Capacity", "Progressive Boot Stealth"):
        assert len(commands_for(name, items[name])) == items[name]


def test_bone_charm_slots_follow_the_games_capacity_steps():
    assert commands_for("Bone Charm Slot", 3) == []  # Corvo's base three slots
    assert commands_for("Bone Charm Slot", 4) == ["upgrade Twk_Upgrade_BoneCharms"]
    assert commands_for("Bone Charm Slot", 5) == ["upgrade Twk_Upgrade_BoneCharms"]  # Piero's 2nd adds two
    assert commands_for("Bone Charm Slot", 6)[-1] == "upgrade Twk_Upgrade_BoneCharms2"
    assert len(commands_for("Bone Charm Slot", 10)) == 6
