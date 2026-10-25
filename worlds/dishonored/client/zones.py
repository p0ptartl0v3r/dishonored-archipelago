"""Which zone key each streaming level needs.

The DLL reports `mapchange <level>` when the player walks into another area; this
table turns the level into the zone key that area needs. Confirmed in-game for the
first two missions (research/notes.md); the rest are taken from the package names and
only listed when the name is unambiguous. Unlisted levels (the bridge parts of The
Royal Physician, parts of the Flooded District) never warn until they're confirmed.
No Archipelago imports here, so tests can load it standalone.
"""

from __future__ import annotations

LEVEL_ZONE_KEYS: dict[str, str] = {
    # Confirmed in-game
    "l_prison_p": "Dunwall Prison Zone Key",
    "l_prsnsewer_p": "Dunwall Sewers Zone Key",
    "l_outsiderdream_p": "The Void Zone Key",
    "l_streets1_p": "Distillery District (HOC) Zone Key",
    "l_distillery_p": "Old Dunwall Distillery (HOC) Zone Key",
    "l_galvani1_p": "Dr Galvani's Offices (HOC) Zone Key",
    "l_ovrsr_p": "High Overseer's Office Zone Key",
    "l_ovrsr_kennel_p": "Kennels Zone Key",
    "l_ovrsr_back_p": "Backyard Zone Key",
    # From the package names, not yet seen in play
    "l_streets2_p": "Distillery District (HoP) Zone Key",
    "l_distillery2_p": "Old Dunwall Distillery (HoP) Zone Key",
    "l_galvani2_p": "Dr Galvani's Offices (HoP) Zone Key",
    "l_artdealer_p": "Art Dealer's Apartment Zone Key",
    "l_brothel_p": "The Golden Cat Bathhouse Zone Key",
    "l_boyle_ext_p": "Lady Boyle's Last Party Exterior Zone Key",
    "l_boyle_int_p": "Boyle Mansion Interior Zone Key",
    "l_towerrtrn_yard_p": "Tower Exterior Zone Key",
    "l_towerrtrn_int_p": "Tower Interior Zone Key",
    "l_flooded_frefinery_p": "Greaves Refinery Zone Key",
    "l_flooded_fgate_p": "Rudshore Gate Zone Key",
    "l_pub_assault_p": "Hound Pits Pub (The Loyalists) Zone Key",
}

# Levels with variants (time of day, chaos level).
LEVEL_PREFIX_ZONE_KEYS: list[tuple[str, str]] = [
    ("l_pub_", "Hound Pits Pub Zone Key"),  # L_Pub_Morning_P, _Day_P, _Dusk_P, _Night_P
    ("l_isl_", "Kingsparrow Fort Zone Key"),  # L_Isl_LowChaos_P / _HighChaos_P
    ("l_lighth_", "Burrows Lighthouse Zone Key"),
]


# Travel prompts ("Go to X?") the player chooses to take: "<choice action path>#<option>"
# (the DLL's `travel` event) -> the level it loads (the `mapchange` that follows). The
# client refuses those leading into a zone whose key hasn't been received. Story
# transitions (cutscenes, mission ends) aren't prompts and are never refused.
def _seq(package: str, choice: int, option: int = 0) -> str:
    return f"{package}.TheWorld.PersistentLevel.Main_Sequence.DisSeqAct_DialogScriptedChoice_{choice}#{option}"


TRAVEL_PROMPTS: dict[str, str] = {
    # High Overseer Campbell (confirmed in-game)
    _seq("L_Streets1_Script", 1): "L_Distillery_P",  # Go to Dunwall Whiskey Distillery
    _seq("L_Distillery_Strt1Script", 1): "L_Streets1_P",  # Go to Bottle Street
    _seq("L_Streets1_Script", 5): "L_Galvani1_P",  # Go to Dr. Galvani's Offices
    _seq("L_Galvani1_Scripts", 4): "L_Streets1_P",  # Go to Clavering Blvd
    _seq("L_Streets1_P", 0): "L_Ovrsr_P",  # Go to Holger Square
    _seq("L_Ovrsr_Script", 4): "L_Streets1_P",  # Go To Distillery District
    _seq("L_Ovrsr_Script", 0): "L_Ovrsr_Kennel_P",  # Go To Kennel
    _seq("L_Ovrsr_Kennel_Script", 4): "L_Ovrsr_P",  # Go To Office of the High Overseer
    _seq("L_Ovrsr_Script", 2): "L_Ovrsr_Back_P",  # Go To Back Yard
    _seq("L_Ovrsr_Back_Script", 4): "L_Ovrsr_P",  # Go to Holger Square
}


def zone_key_for_travel(option: str) -> str | None:
    """The zone key a travel prompt's option leads into, or None if unknown."""
    level = TRAVEL_PROMPTS.get(option)
    return zone_key_for_level(level) if level else None


def zone_key_for_level(level: str) -> str | None:
    """The zone key a streaming level needs, or None if unknown or not a zone (script levels)."""
    name = level.lower()
    if not name.endswith("_p"):
        return None  # L_Pub_FromPrison_Script and the like are sublevels, not areas
    if name in LEVEL_ZONE_KEYS:
        return LEVEL_ZONE_KEYS[name]
    for prefix, key in LEVEL_PREFIX_ZONE_KEYS:
        if name.startswith(prefix):
            return key
    return None
