"""NativeInterface protocol logic, against a fake pipe (the real one is Windows-only)."""

from dishonored.client.game_interface import NativeInterface, event_matches


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
            "gameplay",
            "pickup L_Prison_P.TheWorld.PersistentLevel.DisAbstractItemPickup_3 DisAbstractItemPickup Rune_AbsItm "
            "at=10,20,30 (load)",
            "mapchange L_Out_P",
        ]
    )
    game = attached(pipe)
    game._detectors = [
        ("pickup L_Prison_P.TheWorld.PersistentLevel.DisAbstractItemPickup_3", 111),
        ("mapchange L_Out_P", None),
    ]
    assert game.poll_checked_locations() == {111}
    assert game.poll_goal_reached()
    # Already-read events are not fetched again.
    assert game.poll_checked_locations() == set()
    assert pipe.sent.count("event 0") == 1


EVENT = (
    "pickup DishonoredGameFull_P.TheWorld.PersistentLevel.DisAbstractItemPickup_64 "
    "DisAbstractItemPickup Rune_AbsItm at=1200,-340,95"
)


def test_words_match_exactly():
    assert event_matches("pickup DishonoredGameFull_P.TheWorld.PersistentLevel.DisAbstractItemPickup_64", EVENT)
    assert not event_matches("pickup DishonoredGameFull_P.TheWorld.PersistentLevel.DisAbstractItemPickup_6", EVENT)


def test_near_matches_position():
    assert event_matches("pickup Rune_AbsItm near=1250,-300,100", EVENT)
    assert not event_matches("pickup Rune_AbsItm near=1600,-340,95", EVENT)
    assert not event_matches("pickup Coins_AbsItm near=1200,-340,95", EVENT)


def test_objective_by_class_and_name():
    event = (
        "objective L_Streets1_P.TheWorld.PersistentLevel.Main_Sequence.Objectives_Manager.DisSeqAct_CompleteObjective_2 "
        "DisSeqAct_CompleteObjective Optional_GrannyRagsHook"
    )
    assert event_matches("objective DisSeqAct_CompleteObjective Optional_GrannyRagsHook", event)
    assert not event_matches("objective DisSeqAct_AddObjective Optional_GrannyRagsHook", event)


def test_real_events_from_the_distillery_district():
    """Event lines from an actual play session, matched against locations.json."""
    from dishonored import game_data

    ids = game_data.load().location_name_to_id
    game = NativeInterface()
    prefix = "pickup L_Streets1_Script.TheWorld.PersistentLevel."
    cases = {
        prefix + "DisAbstractItemPickup_19 DisAbstractItemPickup Rune_AbsItm at=0,0,0":
            "Distillery District: Granny Rags Shrine Rune",
        prefix + "DisAbstractItemPickup_26 DisAbstractItemPickup Rune_AbsItm at=0,0,0":
            "Distillery District: Boulevard Confiscated Rune",
        prefix + "DisWhaleBoneCharm_1 DisWhaleBoneCharm WhaleBoneCharm_01_twk at=0,0,0":
            "Distillery District: Griff's Building Bone Charm",
        prefix + "DisWhaleBoneCharm_0 DisWhaleBoneCharm WhaleBoneCharm_01_twk at=0,0,0 (load)":
            "Distillery District: Bridge End Bone Charm",
    }
    for event, location in cases.items():
        assert game.match_event(event) == [ids[location]], event
    # Coins and similar must not match anything.
    assert game.match_event(prefix + "DisAbstractItemPickup_29 DisAbstractItemPickup Coins_AbsItm at=0,0,0") == []
