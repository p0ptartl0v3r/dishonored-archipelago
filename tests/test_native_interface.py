"""NativeInterface protocol logic, against a fake pipe (the real one is Windows-only)."""

from dishonored.client.game_interface import NativeInterface, event_matches


class FakePipe:
    def __init__(self, ready: bool | str = True, events: list[str] | None = None) -> None:
        self.ready = ready
        self.events = events or []
        self.sent: list[str] = []
        self._out = bytearray()

    def write(self, data: bytes) -> None:
        request = data.decode().strip()
        self.sent.append(request)
        if request == "ping":
            reply = self.ready if isinstance(self.ready, str) else ("pong ready" if self.ready else "pong waiting")
        elif request.startswith(("console ", "upgrade ")):
            reply = "queued"
        elif request.startswith("limit "):
            reply = "ok"
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
    spawned = (
        "pickup DishonoredGameFull_P.TheWorld.PersistentLevel.DisAbstractItemPickup_64 DisAbstractItemPickup "
        "Rune_AbsItm at=-6138,-5804,3261"
    )
    assert game.match_event(spawned) == [ids["Distillery District: Granny Rags Gentlemen Callers Reward Rune"]]
    office = "pickup L_Ovrsr_Script.TheWorld.PersistentLevel."
    assert game.match_event(office + "DisAbstractItemPickup_74 DisAbstractItemPickup Rune_AbsItm at=-1712,-27217,-255") == [
        ids["High Overseer's Office: Campbell's Secret Room Rune"]
    ]
    assert game.match_event(office + "DisAbstractItemPickup_2 DisAbstractItemPickup Rune_AbsItm at=-2681,-27365,1542") == [
        ids["High Overseer's Office: Meeting Room Rune"]
    ]
    # Coins and similar must not match anything.
    assert game.match_event(prefix + "DisAbstractItemPickup_29 DisAbstractItemPickup Coins_AbsItm at=0,0,0") == []


def test_upgrade_requests_are_sent_as_is(monkeypatch):
    from dishonored.client import item_commands

    monkeypatch.setitem(item_commands.ITEM_COMMANDS, "Progressive Boot Stealth", item_commands._upgrades("Boots1", "Boots2"))
    pipe = FakePipe()
    assert attached(pipe).give_item(0, "Progressive Boot Stealth", 2)
    assert pipe.sent == ["ping", "upgrade Boots1", "upgrade Boots2"]


def test_waits_while_cheats_are_off():
    pipe = FakePipe(ready="pong nocheats")
    assert not attached(pipe).give_item(0, "Progressive Blink", 1)
    assert pipe.sent == ["ping"]


def test_unverified_commands_are_shown_not_sent(monkeypatch):
    from dishonored.client import game_interface

    monkeypatch.setattr(game_interface, "UNVERIFIED", {"Progressive Blood Thirsty"})
    pipe = FakePipe()
    assert attached(pipe).give_item(0, "Progressive Blood Thirsty", 1)
    assert pipe.sent == []


def test_loading_a_save_requests_a_resync():
    pipe = FakePipe(events=["gameplay", "mapchange L_Ovrsr_P", "gameplay"])
    game = attached(pipe)
    game.poll_checked_locations()
    assert game.take_resync_request()
    assert not game.take_resync_request()  # only once per load


def test_replayed_manual_unlocks_are_not_repeated(caplog):
    game = attached(FakePipe())
    with caplog.at_level("INFO"):
        game.give_item(0, "Kennels Zone Key", 1)
        game.give_item(0, "Kennels Zone Key", 1)
    assert sum("Unlocked: Kennels Zone Key" in r.message for r in caplog.records) == 1


def test_real_events_from_the_first_mission():
    """Event lines from a playthrough of the prologue, Coldridge, the sewers, the Void and the pub."""
    from dishonored import game_data

    ids = game_data.load().location_name_to_id
    game = NativeInterface()
    prison = "pickup L_Prison_Script.TheWorld.PersistentLevel."
    full = "pickup DishonoredGameFull_P.TheWorld.PersistentLevel."
    cases = {
        "objective L_Tower_Script.TheWorld.PersistentLevel.Main_Sequence.==Objectives==.DisSeqAct_CompleteObjective_3 "
        "DisSeqAct_CompleteObjective TowerEmpress_HideNSeek": "Prologue: Hide And Seek",
        "mapchange L_Prison_P": "Prologue: Get Framed",
        prison + "DisKey_Base_1 DisKey_Base - at=64,342,1075": "Dishonored (Mission): Corvo's Cell Key",
        prison + "DisAbstractItemPickup_0 DisAbstractItemPickup ExplosiveDevice_AbsItm at=-843,9412,1149":
            "Dishonored (Mission): Clockwork Explosive",
        "objective L_PrsnSewer_Script.TheWorld.PersistentLevel.Main_Sequence.DisSeqAct_CompleteObjective_0 "
        "DisSeqAct_CompleteObjective Prison_Escape": "Dishonored (Mission): Victory",
        "pickup L_OutsiderDream_Script.TheWorld.PersistentLevel.DisAbstractItemPickup_0 DisAbstractItemPickup "
        "Rune_AbsItm at=8881,1236,-2484": "The Void: Outsider's Gift Rune",
        full + "DisAbstractItemPickup_155 DisAbstractItemPickup Rune_AbsItm at=-11585,-5794,-528": "The Pub: River Rune",
        "buy item Rune_AbsItm for 500 at=-1282,-3174,386": "The Pub: Shop Rune",
        "grant inventory Twk_Inv_CrossbowCorvo by L_PrsnSewer_Script.TheWorld.PersistentLevel.Main_Sequence."
        "Corvo_SpecialGear.DisSeqAct_AddInventoryItem_3": "Dishonored (Mission): Folding Blade and Crossbow",
        "pickup L_PrsnSewer_Script.TheWorld.PersistentLevel.DisAbstractItemPickup_79 DisAbstractItemPickup "
        "Coins_AbsItm at=6578,4392,3424": "Dishonored (Mission): Open Jelly's Safe",
        "pickup L_PrsnSewer_Script.TheWorld.PersistentLevel.DisElixirHealth_1 DisElixirHealth - at=6556,4379,3455":
            "Dishonored (Mission): Open Jelly's Safe",
        "grant power Blink 1 by L_OutsiderDream_Script.TheWorld.PersistentLevel.Main_Sequence.Outsider_Intro_Blink."
        "DisSeqAct_AddPower_0": "The Void: Receive the Mark",
    }
    for event, location in cases.items():
        assert game.match_event(event) == [ids[location]], event
    # Other prison keys, the objective failing after hide and seek, and coins elsewhere match nothing.
    assert game.match_event(prison + "DisKey_Base_2 DisKey_Base - at=2478,3354,1640") == []
    assert game.match_event("grant inventory Twk_Inv_SwordCorvo by x") == []  # same cutscene, one check
    assert game.match_event(
        "objective L_Tower_Script.TheWorld.PersistentLevel.Main_Sequence.==Objectives==.DisSeqAct_FailObjective_0 "
        "DisSeqAct_FailObjective TowerEmpress_HideNSeek"
    ) == []
    assert game.match_event(prison + "DisAbstractItemPickup_27 DisAbstractItemPickup Coins_AbsItm at=104,1144,1107") == []


def test_limits_are_sent_as_is():
    pipe = FakePipe()
    assert attached(pipe).set_limits(["limit power Blink 1", "limit upgrade Twk_Upgrade_Armor 0"])
    assert pipe.sent == ["limit power Blink 1", "limit upgrade Twk_Upgrade_Armor 0"]
