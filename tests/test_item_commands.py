import json
from pathlib import Path

from dishonored.client.item_commands import ITEM_COMMANDS, UNVERIFIED, _upgrades, commands_for, console_commands, limits

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


def test_limits_follow_what_was_received():
    lines = limits({"Progressive Blink": 1, "Progressive Pistol Accuracy": 1, "Bone Charm Slot": 4})
    assert "limit power Blink 1" in lines
    assert "limit power WindBlast 0" in lines  # nothing received: none allowed
    assert "limit upgrade Twk_Upgrade_Pistol_Accuracy1 1" in lines
    assert "limit upgrade Twk_Upgrade_Pistol_Accuracy2 0" in lines
    assert "limit upgrade Twk_Upgrade_BoneCharms 1" in lines
    assert "limit upgrade Twk_Upgrade_StatueFish 0" in lines
    assert "limit upgrade Twk_Upgrade_Sword 0" in lines  # the second Progressive Sword
    assert not any("Twk_Upgrade_Mask" in line for line in lines)  # not randomized: never limited


def test_extra_copies_never_exceed_the_maximum():
    assert "limit power Blink 2" in limits({"Progressive Blink": 5})


def test_bone_charm_base_makes_the_total_match_the_items():
    from dishonored.client.item_commands import bone_charm_base

    assert [bone_charm_base(n) for n in range(11)] == [0, 1, 2, 3, 3, 4, 3, 3, 3, 3, 3]
    assert "limit bonecharms base 1" in limits({"Bone Charm Slot": 1})


def test_equipment_allows_ammo_types_in_order():
    lines = limits({"Progressive Bullets": 1, "Progressive Grenades": 2})
    assert "limit ammo Bullet 1" in lines
    assert "limit ammo Bullet_Explosive 0" in lines
    assert "limit ammo Grenade 1" in lines and "limit ammo StickyGrenade 1" in lines
    assert "limit ammo Arrow 0" in lines and "limit ammo Arrow_Sleep 0" in lines
    assert "limit item RewireTool 0" in lines
    assert "limit item RewireTool_AbsItm 1" in limits({"Rewire Tool": 1})
