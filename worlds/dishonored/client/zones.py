"""Which zone key each streaming level needs.

The DLL reports `mapchange <level>` when the player walks into another area; this
table turns the level into the zone key that area needs. Confirmed in-game for the
first five missions (research/notes.md); the rest are taken from the package names and
only listed when the name is unambiguous. Unlisted levels (parts of the Flooded District)
never warn until they're confirmed.
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
    "l_streets2_p": "Distillery District (HoP) Zone Key",
    "l_distillery2_p": "Old Dunwall Distillery (HoP) Zone Key",
    "l_galvani2_p": "Dr Galvani's Offices (HoP) Zone Key",
    "l_artdealer_p": "Art Dealer's Apartment Zone Key",
    "l_brothel_p": "The Golden Cat Bathhouse Zone Key",
    "l_bridge_part1a_p": "Southside Gate Zone Key",
    "l_bridge_part1b_p": "Drawbridge Control Zone Key",
    "l_bridge_part1c_p": "Midrow Substation Zone Key",
    "l_bridge_part2_p": "North End Zone Key",
    "l_boyle_ext_p": "Lady Boyle's Last Party Exterior Zone Key",
    "l_boyle_int_p": "Boyle Mansion Interior Zone Key",
    "l_towerrtrn_yard_p": "Tower Exterior Zone Key",  # the rooftop is part of it too
    "l_towerrtrn_int_p": "Tower Interior Zone Key",
    # From the package names, not yet seen in play
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
# (the DLL's `travel` event) -> the level it loads (the `mapchange` that follows, or the
# prompt's text for doors only seen in `dump`). The client refuses those leading into a
# zone whose key hasn't been received. Story transitions (cutscenes, mission ends) aren't
# prompts and are never refused. A level script used by two missions (Galvani's offices)
# has its prompts listed per area: "<area level>|<option>".
def _seq(package: str, choice: int, option: int = 0, area: str = "") -> str:
    key = f"{package}.TheWorld.PersistentLevel.Main_Sequence.DisSeqAct_DialogScriptedChoice_{choice}#{option}"
    return f"{area}|{key}" if area else key


TRAVEL_PROMPTS: dict[str, str] = {
    # High Overseer Campbell (confirmed in-game)
    _seq("L_Streets1_Script", 1): "L_Distillery_P",  # Go to Dunwall Whiskey Distillery
    _seq("L_Distillery_Strt1Script", 1): "L_Streets1_P",  # Go to Bottle Street
    _seq("L_Streets1_Script", 5): "L_Galvani1_P",  # Go to Dr. Galvani's Offices
    _seq("L_Galvani1_Scripts", 4, area="L_Galvani1_P"): "L_Streets1_P",  # Go to Clavering Blvd
    _seq("L_Streets1_P", 0): "L_Ovrsr_P",  # Go to Holger Square
    _seq("L_Ovrsr_Script", 4): "L_Streets1_P",  # Go To Distillery District
    _seq("L_Ovrsr_Script", 0): "L_Ovrsr_Kennel_P",  # Go To Kennel
    _seq("L_Ovrsr_Kennel_Script", 4): "L_Ovrsr_P",  # Go To Office of the High Overseer
    _seq("L_Ovrsr_Script", 2): "L_Ovrsr_Back_P",  # Go To Back Yard
    _seq("L_Ovrsr_Back_Script", 4): "L_Ovrsr_P",  # Go to Holger Square
    _seq("L_Streets1_Script", 4): "L_Galvani1_P",  # Go to Dr. Galvani's Offices (2nd door)
    _seq("L_Streets1_Script", 6): "L_Galvani1_P",  # Go to Dr. Galvani's Offices (3rd door)
    _seq("L_Galvani1_Scripts", 3, area="L_Galvani1_P"): "L_Streets1_P",  # Go to Clavering Blvd
    _seq("L_Galvani1_Scripts", 1, area="L_Galvani1_P"): "L_Streets1_P",  # Go to Clavering Blvd
    _seq("L_Ovrsr_Script", 3): "L_Ovrsr_Kennel_P",  # Go To Kennel (2nd door)
    _seq("L_Ovrsr_Kennel_Script", 2): "L_Ovrsr_P",  # Go To Office of the High Overseer
    _seq("L_Ovrsr_Script", 1): "L_Ovrsr_Back_P",  # Go To Back Yard (2nd door)
    _seq("L_Ovrsr_Script", 9): "L_Ovrsr_Back_P",  # Go To Back Yard (3rd door; seen in dump only)
    _seq("L_Ovrsr_Back_Script", 5): "L_Ovrsr_P",  # Go to The Office of the High Overseer
    # House of Pleasure (taken in play, or from `dump` with the destination from the text)
    _seq("L_Streets2_Script", 1): "L_Distillery2_P",  # Go to Dunwall Whiskey Distillery
    _seq("L_Distillery2_Script", 1): "L_Streets2_P",  # Go to Bottle Street
    _seq("L_Streets2_Script", 2): "L_ArtDealer_P",  # Go to Art Dealer's Apartment
    _seq("L_Streets2_Script", 3): "L_ArtDealer_P",  # Go to Art Dealer's Apartment
    _seq("L_Streets2_Script", 7): "L_ArtDealer_P",  # Go to Art Dealer's Apartment (dump)
    _seq("L_Streets2_Script", 8): "L_ArtDealer_P",  # Go to Art Dealer's Apartment (dump)
    _seq("L_ArtDealer_Scripts", 0): "L_Streets2_P",  # Go to Clavering Blvd
    _seq("L_ArtDealer_Scripts", 1): "L_Streets2_P",  # Go to Bottle Street
    _seq("L_ArtDealer_Scripts", 2): "L_Streets2_P",  # Go to Clavering Blvd (dump)
    _seq("L_ArtDealer_Scripts", 3): "L_Streets2_P",  # Go to Clavering Blvd (dump)
    _seq("L_Streets2_Script", 4): "L_Galvani2_P",  # Go to Dr. Galvani's Offices.
    _seq("L_Streets2_Script", 5): "L_Galvani2_P",  # Go to Dr. Galvani's Offices.
    _seq("L_Streets2_Script", 6): "L_Galvani2_P",  # Go to Dr. Galvani's Offices. (dump)
    _seq("L_Galvani1_Scripts", 1, area="L_Galvani2_P"): "L_Streets2_P",  # Go to Clavering Blvd
    _seq("L_Galvani1_Scripts", 3, area="L_Galvani2_P"): "L_Streets2_P",  # Go to Clavering Blvd
    _seq("L_Galvani1_Scripts", 4, area="L_Galvani2_P"): "L_Streets2_P",  # Go to Clavering Blvd (dump)
    _seq("L_Streets2_Script", 10): "L_Brothel_P",  # Go to the Golden Cat
    _seq("L_Streets2_Script", 11): "L_Brothel_P",  # Go to the Golden Cat (dump)
    _seq("L_Brothel_Script", 5): "L_Streets2_P",  # Go To Distillery District
    _seq("L_Brothel_Script", 2): "L_Streets2_P",  # Go to Clavering Blvd (dump)
    _seq("L_Brothel_Script", 4): "L_Streets2_P",  # Go to Captain's Chair Hotel (dump)
    # The Royal Physician (taken in play, or from `dump` with the destination from the text)
    _seq("L_Bridge_Part1a_Script", 2): "L_Bridge_Part1b_P",  # Go to Drawbridge Way
    _seq("L_Bridge_Part1b_Script", 1): "L_Bridge_Part1a_P",  # Go to Southside Gate (dump)
    _seq("L_Bridge_Part1b_Script", 2): "L_Bridge_Part1c_P",  # Go to Midrow Substation
    _seq("L_Bridge_Part1c_Script", 1): "L_Bridge_Part2_P",  # Go to North End
    _seq("L_Bridge_Part1c_Script", 2): "L_Bridge_Part1b_P",  # Go to Drawbridge Way (dump)
    _seq("L_Bridge_Part2_Script", 2): "L_Bridge_Part1c_P",  # Go to Midrow Substation (dump)
    # Lady Boyle's Last Party (taken in play; no dump yet, so other doors may be missing)
    _seq("L_Boyle_Ext_P", 4): "L_Boyle_Int_P",  # Go to Boyle Mansion Foyer
    _seq("L_Boyle_Ext_P", 0): "L_Boyle_Int_P",  # Go to Boyle Mansion Cellars
    _seq("L_Boyle_Int_P", 1): "L_Boyle_Ext_P",  # Go to the Sewers
    _seq("L_Boyle_Int_P", 2): "L_Boyle_Ext_P",  # Go to Boyle Gardens
    _seq("L_Boyle_Ext_P", 5): "L_Boyle_Int_P",  # Go to Mansion Second Floor (Forbidden)
    # Return to the Tower (from `dump` in both areas; 5 and 2 taken in play)
    _seq("L_TowerRtrn_Yard_Script", 3): "L_TowerRtrn_Int_P",  # Go to Dunwall Tower Interior
    _seq("L_TowerRtrn_Yard_Script", 5): "L_TowerRtrn_Int_P",  # Go to Dunwall Tower Interior
    _seq("L_TowerRtrn_Yard_Script", 6): "L_TowerRtrn_Int_P",  # Go to Dunwall Tower Interior
    _seq("L_TowerRtrn_Yard_Script", 7): "L_TowerRtrn_Int_P",  # Go to Dunwall Tower Interior
    _seq("L_TowerRtrn_Int_Script", 0): "L_TowerRtrn_Yard_P",  # Go to Dunwall Tower Exterior
    _seq("L_TowerRtrn_Int_Script", 7): "L_TowerRtrn_Yard_P",  # Go to Dunwall Tower Exterior
    _seq("L_TowerRtrn_Int_Script", 8): "L_TowerRtrn_Yard_P",  # Go to Dunwall Tower Exterior
    _seq("L_TowerRtrn_Int_Script", 5): "L_TowerRtrn_Yard_P",  # Go to Dunwall Tower Rooftop
}


def zone_key_for_travel(option: str, areas: str = "") -> str | None:
    """The zone key a travel prompt's option leads into, or None if unknown. ``option`` is a
    table key, or a bare option with the comma-separated ``areas`` the player was in."""
    level = TRAVEL_PROMPTS.get(option)
    for area in areas.split(","):
        level = level or TRAVEL_PROMPTS.get(f"{area}|{option}")
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
