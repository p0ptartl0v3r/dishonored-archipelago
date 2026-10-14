"""NativeInterface protocol logic, against a fake pipe (the real one is Windows-only)."""

from dishonored.client.game_interface import NativeInterface


class FakePipe:
    def __init__(self, ready: bool = True, events: list[str] | None = None) -> None:
        self.ready = ready
        self.events = events or []
        self.sent: list[str] = []
        self._out = bytearray()

    def write(self, data: bytes) -> None:
        request = data.decode().strip()
        self.sent.append(request)
        if request == "ping":
            reply = "pong ready" if self.ready else "pong waiting"
        elif request.startswith("console "):
            reply = "queued"
        elif request.startswith("event "):
            index = int(request.split()[1])
            reply = f"event {index} {self.events[index]}" if index < len(self.events) else "none"
        else:
            reply = "error unknown request"
        self._out += (reply + "\n").encode()

    def read(self, n: int) -> bytes:
        chunk, self._out = bytes(self._out[:n]), self._out[n:]
        return chunk

    def close(self) -> None:
        pass


def attached(pipe: FakePipe) -> NativeInterface:
    game = NativeInterface()
    game._pipe = pipe
    return game


def test_power_is_sent_as_console_command():
    pipe = FakePipe()
    assert attached(pipe).give_item(0, "Progressive Blink", 2)
    assert pipe.sent == ["ping", "console AddPower Blink 2"]


def test_waits_while_not_in_game():
    pipe = FakePipe(ready=False)
    assert not attached(pipe).give_item(0, "Progressive Blink", 1)
    assert pipe.sent == ["ping"]


def test_items_without_commands_need_no_game():
    pipe = FakePipe(ready=False)
    assert attached(pipe).give_item(0, "Whalebone", 1)
    assert pipe.sent == []


def test_lost_pipe_detaches_and_retries():
    class BrokenPipe(FakePipe):
        def write(self, data: bytes) -> None:
            raise OSError("gone")

    game = attached(BrokenPipe())
    assert not game.give_item(0, "Progressive Blink", 1)
    assert not game.is_attached()


def test_events_become_locations_and_goal():
    pipe = FakePipe(
        events=[
            "gameplay L_Prison_P",
            "pickup L_Prison_P.TheWorld.PersistentLevel.DisAbstractItemPickup_3 DisAbstractItemPickup Rune (load)",
            "mapchange L_Out_P",
        ]
    )
    game = attached(pipe)
    game._detectors = [
        ("pickup L_Prison_P.TheWorld.PersistentLevel.DisAbstractItemPickup_3 ", 111),
        ("mapchange L_Out_", None),
    ]
    assert game.poll_checked_locations() == {111}
    assert game.poll_goal_reached()
    # Already-read events are not fetched again.
    assert game.poll_checked_locations() == set()
    assert pipe.sent.count("event 0") == 1
