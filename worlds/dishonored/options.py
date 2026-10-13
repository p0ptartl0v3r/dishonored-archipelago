from dataclasses import dataclass

from Options import DeathLink, PerGameCommonOptions


@dataclass
class DishonoredOptions(PerGameCommonOptions):
    death_link: DeathLink
