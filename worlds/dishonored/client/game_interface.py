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

from ..game_data import load as load_game_data
from .item_commands import UNVERIFIED, commands_for

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
    def give_item(self, item_id: int, item_name: str, level: int) -> bool:
        """Apply one received item in the game. Return False to retry later.

        ``level`` counts copies of this item received so far, including this one.
        """


def log_unlock_for_player(item_name: str, level: int) -> None:
    """Tell the player what they got and, if known, what to type in the console."""
    logger.info(f"Unlocked: {item_name}" + (f" ({level})" if level > 1 else ""))
    for command in commands_for(item_name, level):
        note = " (unverified)" if item_name in UNVERIFIED else ""
        logger.info(f"    console: {command}{note}")


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

    def give_item(self, item_id: int, item_name: str, level: int) -> bool:
        log_unlock_for_player(item_name, level)
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

    def give_item(self, item_id: int, item_name: str, level: int) -> bool:
        # TODO: run item_commands.commands_for(...) in-game. Until then, behave like the Manual.
        log_unlock_for_player(item_name, level)
        return True


class NativeInterface(GameInterface):
    """Talks to the in-game DLL (native/) over a named pipe.

    Locations with "detect" patterns in locations.json are sent when the DLL reports
    a matching game event. Items with a known console command are granted
    automatically; everything else is shown to the player as before.
    """

    name = "native"
    PIPE_PATH = r"\\.\pipe\dishonored_archipelago"

    def __init__(self) -> None:
        self._pipe = None
        self._next_event = 0
        self._goal_reached = False
        data = load_game_data()
        # (event prefix, location id or None for the goal)
        self._detectors: list[tuple[str, int | None]] = [
            (pattern, location.id) for location in data.locations.values() for pattern in location.detect
        ]

    def attach(self) -> bool:
        try:
            self._pipe = open(self.PIPE_PATH, "r+b", buffering=0)
            # The DLL keeps every event since the game started; replay them all
            # (the server ignores locations it already has).
            self._next_event = 0
            self._request("ping")
        except OSError:
            self._pipe = None
            return False
        logger.info("Connected to the Dishonored native module")
        return True

    def is_attached(self) -> bool:
        return self._pipe is not None

    def detach(self) -> None:
        if self._pipe is not None:
            try:
                self._pipe.close()
            except OSError:
                pass
        self._pipe = None

    def _request(self, line: str) -> str:
        """Send one request line and return the reply. Raises OSError if the game went away."""
        if self._pipe is None:
            raise OSError("not attached")
        try:
            self._pipe.write((line + "\n").encode("utf-8"))
            reply = bytearray()
            while not reply.endswith(b"\n"):
                chunk = self._pipe.read(1)
                if not chunk:
                    raise OSError("pipe closed")
                reply += chunk
        except OSError:
            self.detach()
            raise
        return reply.decode("utf-8").strip()

    def _ready(self) -> bool:
        return self._request("ping") == "pong ready"

    def match_event(self, text: str) -> list[int | None]:
        """Location IDs (None = goal) that a game event completes."""
        text = text.removesuffix(" (load)")
        return [location for pattern, location in self._detectors if text.startswith(pattern)]

    def poll_checked_locations(self) -> set[int]:
        found: set[int] = set()
        try:
            while (reply := self._request(f"event {self._next_event}")) != "none":
                _, _, text = reply.split(" ", 2)
                self._next_event += 1
                matches = self.match_event(text)
                if not matches:
                    # Shown so you can copy it into a location's "detect" list.
                    logger.info(f"Game event (no location): {text}")
                for location in matches:
                    if location is None:
                        self._goal_reached = True
                    else:
                        found.add(location)
        except (OSError, ValueError):
            pass
        return found

    def poll_goal_reached(self) -> bool:
        return self._goal_reached

    def give_item(self, item_id: int, item_name: str, level: int) -> bool:
        commands = commands_for(item_name, level)
        if not commands:
            log_unlock_for_player(item_name, level)
            return True
        try:
            # Only send while in-game; in menus or during loading, retry later.
            if not self._ready():
                return False
            for command in commands:
                if self._request(f"console {command}") != "queued":
                    return False
        except OSError:
            return False
        logger.info(f"Unlocked: {item_name}" + (f" ({level})" if level > 1 else "") + " (applied in-game)")
        return True


BACKENDS: dict[str, type[GameInterface]] = {
    ManualInterface.name: ManualInterface,
    MemoryInterface.name: MemoryInterface,
    NativeInterface.name: NativeInterface,
}
