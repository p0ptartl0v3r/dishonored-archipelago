from dataclasses import dataclass

from Options import Choice, DeathLink, DefaultOnToggle, PerGameCommonOptions


class VoidWalkersArsenal(DefaultOnToggle):
    """You own the Void Walker's Arsenal DLC. Its four bone charm statues raise the bone charm
    slot limit from 6 to 10, so the pool has 10 Bone Charm Slot items with it and 6 without."""

    display_name = "Void Walker's Arsenal DLC"


class StartingZoneKeys(Choice):
    """Zone keys you start with. The story moves you on to the next area by itself (the end of
    the Prologue takes you to Dunwall Prison), and being in an area without its key sends you
    back to the main menu, so starting with the first keys avoids waiting on other players.

    none: like the Manual, every zone key is shuffled.
    prison: start with the Dunwall Prison Zone Key.
    intro: start with the keys for Dunwall Prison, the Sewers, the Hound Pits Pub and the Void."""

    display_name = "Starting Zone Keys"
    option_none = 0
    option_prison = 1
    option_intro = 2
    default = 0


STARTING_ZONE_KEYS = {
    StartingZoneKeys.option_none: [],
    StartingZoneKeys.option_prison: ["Dunwall Prison Zone Key"],
    StartingZoneKeys.option_intro: [
        "Dunwall Prison Zone Key",
        "Dunwall Sewers Zone Key",
        "Hound Pits Pub Zone Key",
        "The Void Zone Key",
    ],
}

BONE_CHARM_SLOTS_WITHOUT_DLC = 6


@dataclass
class DishonoredOptions(PerGameCommonOptions):
    void_walkers_arsenal: VoidWalkersArsenal
    starting_zone_keys: StartingZoneKeys
    death_link: DeathLink
