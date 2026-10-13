"""The boundary between the Archipelago client and the running game.

The client only ever talks to the game through a ``GameInterface``. Today the
only working backend is ``ManualInterface`` (nothing is read from the game; the
player sends checks with ``/check``). ``MemoryInterface`` is where real
detection goes once the research in ``research/`` has found what to read. If
the work later moves into an injected DLL, a third backend that talks to that
DLL over a pipe or socket slots in here without the client changing.
"""

from __future__ import annotations

import logging
from abc import ABC, abstractmethod

logger = logging.getLogger("DishonoredClient")


class GameInterface(ABC):
    name: str

    @abstractmethod
    def attach(self) -> bool:
        """Try to connect to the game. Return True once connected."""

    @abstractmethod
    def is_attached(self) -> bool: ...

    @abstractmethod
    def detach(self) -> None: ...

    @abstractmethod
    def poll_checked_locations(self) -> set[int]:
        """Location IDs the game reports as done. May repeat earlier ones."""

    @abstractmethod
    def poll_goal_reached(self) -> bool: ...

    @abstractmethod
    def give_item(self, item_id: int, item_name: str) -> bool:
        """Apply one received item in the game. Return False to retry later."""


class ManualInterface(GameInterface):
    """No game connection: the player follows the rules by hand, like the Manual."""

    name = "manual"

    def attach(self) -> bool:
        return True

    def is_attached(self) -> bool:
        return True

    def detach(self) -> None:
        pass

    def poll_checked_locations(self) -> set[int]:
        return set()

    def poll_goal_reached(self) -> bool:
        return False

    def give_item(self, item_id: int, item_name: str) -> bool:
        logger.info(f"Unlocked: {item_name}")
        return True


class MemoryInterface(GameInterface):
    """Reads and writes Dishonored's memory from outside the process (pymem).

    Nothing is implemented yet: each TODO below is a research task. Dishonored
    is a 32-bit process, and addresses are only valid for one game build, so
    record the build you found them on in research/.
    """

    name = "memory"
    PROCESS_NAME = "Dishonored.exe"

    def __init__(self) -> None:
        self._pm = None

    def attach(self) -> bool:
        try:
            import pymem
        except ImportError:
            logger.error("pymem is not installed")
            return False
        try:
            self._pm = pymem.Pymem(self.PROCESS_NAME)
        except Exception:
            self._pm = None
            return False
        logger.info(f"Attached to {self.PROCESS_NAME} (pid {self._pm.process_id})")
        return True

    def is_attached(self) -> bool:
        if self._pm is None:
            return False
        try:
            # Any read fails once the game has exited.
            self._pm.read_bytes(self._pm.base_address, 1)
            return True
        except Exception:
            self._pm = None
            return False

    def detach(self) -> None:
        if self._pm is not None:
            self._pm.close_process()
        self._pm = None

    def poll_checked_locations(self) -> set[int]:
        # TODO: mission completion first (current map name + end-of-mission state),
        #       then individual pickups (runes, bone charms, paintings, blueprints).
        return set()

    def poll_goal_reached(self) -> bool:
        # TODO: detect finishing "The Light at the End".
        return False

    def give_item(self, item_id: int, item_name: str) -> bool:
        # TODO: grant powers/equipment/upgrades. Until then, behave like the Manual.
        logger.info(f"Unlocked (apply by hand for now): {item_name}")
        return True


BACKENDS: dict[str, type[GameInterface]] = {
    ManualInterface.name: ManualInterface,
    MemoryInterface.name: MemoryInterface,
}
