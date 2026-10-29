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
        elif request.startswith(("limit ", "message ")):
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


def test_messages_are_sent_on_one_line():
    pipe = FakePipe()
    assert attached(pipe).show_message("Alice sent Progressive Blink\nto Bob")
    assert pipe.sent == ["message Alice sent Progressive Blink to Bob"]


def test_real_events_from_high_overseer_campbell():
    """Event lines from a playthrough of the second mission (2026-10-25)."""
    from dishonored import game_data

    ids = game_data.load().location_name_to_id
    game = NativeInterface()
    back = "pickup L_Ovrsr_Back_Script.TheWorld.PersistentLevel."
    full = "pickup DishonoredGameFull_P.TheWorld.PersistentLevel."
    cases = {
        "objective L_Streets1_P.TheWorld.PersistentLevel.Main_Sequence.Objectives_Manager.DisSeqAct_CompleteObjective_0 "
        "DisSeqAct_CompleteObjective Optional_HelpGrannyRags": "Distillery District: Granny Rags Poison Bootleg Elixer Reward Rune",
        "buy item BP_SpyglassZoom_AbsItm for 200 at=-2402,-2164,2452":
            "Distillery District: Griff's Shop Lens Magnification Blueprint",
        "buy item BP_HealthPotion_AbsItm for 200 at=-2402,-2164,2452":
            "Distillery District: Griff's Shop Sokolov's Formula Blueprint",
        "pickup L_Distillery_Strt1Script.TheWorld.PersistentLevel.DisWhaleBoneCharm_1 DisWhaleBoneCharm "
        "WhaleBoneCharm_01_twk at=3159,-6086,2046": "Old Dunwall Distillery: Rafter Bone Charm",
        "pickup L_Ovrsr_Script.TheWorld.PersistentLevel.DisAbstractItemPickup_29 DisAbstractItemPickup Coins_AbsItm "
        "at=-1335,-27798,-356": "High Overseer's Office: Campbell's Secret Room Painting",
        back + "DisAbstractItemPickup_2 DisAbstractItemPickup Rune_AbsItm at=-5770,-30442,-50": "Backyard: Workshop Rune",
        back + "DisAbstractItemPickup_65 DisAbstractItemPickup BP_ArmorIncrease_AbsItm at=-6395,-29809,-59":
            "Backyard: Bonded Galvani Weave Blueprint",
        back + "DisWhaleBoneCharm_0 DisWhaleBoneCharm WhaleBoneCharm_01_twk at=-1380,-35481,-537":
            "Backyard: Overseer Stolen Bone Charm",
        # The vise releases a loose charm spawned at runtime: its number isn't stable.
        "pickup DishonoredGameFull_P.TheWorld.PersistentLevel.DisWhaleBoneCharm_3 DisWhaleBoneCharm "
        "Ovrsr_PhysBoneCharm at=-10708,-30313,-107": "Backyard: Rat Infested Building Bone Charm",
        "objective L_Ovrsr_Back_Script.TheWorld.PersistentLevel.Main_Sequence.DisSeqAct_CompleteObjective_1 "
        "DisSeqAct_CompleteObjective Strt1Obj_ReturnToSamuel": "High Overseer Campbell: Victory",
        full + "DisAbstractItemPickup_155 DisAbstractItemPickup Rune_AbsItm at=1128,-3197,-619": "The Pub: Weeper Chest Rune",
        full + "DisAbstractItemPickup_156 DisAbstractItemPickup Rune_AbsItm at=1638,-3551,-870":
            "The Pub: Weeper Underwater Rune",
    }
    for event, location in cases.items():
        assert game.match_event(event) == [ids[location]], event
    # The meeting room rune (_2 in the office) must not be confused with the painting (_29).
    assert game.match_event("pickup L_Ovrsr_Script.TheWorld.PersistentLevel.DisAbstractItemPickup_2 x") == [
        ids["High Overseer's Office: Meeting Room Rune"]
    ]
    # Opening the vise destroys the static charm; only picking up the loose one counts.
    assert game.match_event(
        "pickup L_Ovrsr_Back_Script.TheWorld.PersistentLevel.DisWhaleBoneCharm_2 DisWhaleBoneCharm "
        "Ovrsr_NoPhysBoneCharm at=-10651,-30305,21"
    ) == []


def test_only_the_latest_settled_zone_is_reported():
    pipe = FakePipe(events=["mapchange L_Streets1_P", "area L_Streets1_P", "mapchange L_Ovrsr_P", "area L_Ovrsr_P"])
    game = attached(pipe)
    game.poll_checked_locations()
    assert game.take_zone_entered() == "L_Ovrsr_P"
    assert game.take_zone_entered() is None


def test_a_zone_left_before_it_settled_is_not_reported():
    # Quitting to the main menu (or travelling on) after the area event cancels it.
    pipe = FakePipe(events=["area L_Ovrsr_Kennel_P", "mapchange Dishonored_MainMenu"])
    game = attached(pipe)
    game.poll_checked_locations()
    assert game.take_zone_entered() is None


def test_areas_without_a_zone_are_ignored():
    pipe = FakePipe(events=["area L_Tower_P", "area -"])
    game = attached(pipe)
    game.poll_checked_locations()
    assert game.take_zone_entered() is None


def test_leaving_the_area_is_reported():
    pipe = FakePipe(events=["area L_Ovrsr_Kennel_P", "mapchange L_Ovrsr_P"])
    game = attached(pipe)
    game.poll_checked_locations()
    assert game.take_zone_left()
    assert not game.take_zone_left()


def test_real_events_from_house_of_pleasure():
    """Event lines from a playthrough of the third mission (2026-10-26)."""
    from dishonored import game_data

    ids = game_data.load().location_name_to_id
    game = NativeInterface()
    streets = "pickup L_Streets2_Script.TheWorld.PersistentLevel."
    distillery = "pickup L_Distillery2_Script.TheWorld.PersistentLevel."
    dealer = "pickup L_ArtDealer_Scripts.TheWorld.PersistentLevel."
    cat = "pickup L_Brothel_Script.TheWorld.PersistentLevel."
    cases = {
        streets + "DisWhaleBoneCharm_0 DisWhaleBoneCharm WhaleBoneCharm_01_twk at=-5296,-660,4017":
            "Distillery District: Apartment Bone Charm",
        streets + "DisWhaleBoneCharm_1 DisWhaleBoneCharm WhaleBoneCharm_01_twk at=-1373,4994,1868":
            "Distillery District: Granny Rags Bone Charm",
        streets + "DisWhaleBoneCharm_4 DisWhaleBoneCharm WhaleBoneCharm_01_twk at=-1434,8922,3447":
            "Distillery District: Captain's Chair Hotel Bone Charm",
        distillery + "DisAbstractItemPickup_213 DisAbstractItemPickup BP_FlareBolt_AbsItm at=2059,-1040,1139":
            "Old Dunwall Distillery: Incandescent Paste Blueprint",
        distillery + "DisWhaleBoneCharm_0 DisWhaleBoneCharm WhaleBoneCharm_01_twk at=529,-471,960":
            "Old Dunwall Distillery: Weeper Containment Room Bone Charm",
        "pickup L_Galvani1_Strt2_Scripts.TheWorld.PersistentLevel.DisAbstractItemPickup_32 DisAbstractItemPickup "
        "Rune_AbsItm at=2341,-33,4785": "Dr Galvani's Offices: Galvani's Secret Room Rune",
        # Paintings are valuables: they turn into 300 coins.
        dealer + "DisAbstractItemPickup_6 DisAbstractItemPickup Coins_AbsItm at=-2342,-986,3971":
            "Art Dealer's Apartment: Second Floor Painting",
        dealer + "DisAbstractItemPickup_4 DisAbstractItemPickup Coins_AbsItm at=-1978,1338,4683":
            "Art Dealer's Apartment: Third Floor Painting",
        dealer + "DisAbstractItemPickup_5 DisAbstractItemPickup Rune_AbsItm at=-1635,-928,4786":
            "Art Dealer's Apartment: Art Dealer's Safe",
        dealer + "DisAbstractItemPickup_9 DisAbstractItemPickup Coins_AbsItm at=-1475,-929,4822":
            "Art Dealer's Apartment: Inside Safe Painting",
        cat + "DisAbstractItemPickup_17 DisAbstractItemPickup Rune_AbsItm at=39957,10780,4377":
            "The Golden Cat Bathhouse: Opposite Golden Cat Rune",
        cat + "DisAbstractItemPickup_60 DisAbstractItemPickup Rune_AbsItm at=35647,10644,5485":
            "The Golden Cat Bathhouse: Courtesans Room Rune",
        cat + "DisWhaleBoneCharm_0 DisWhaleBoneCharm WhaleBoneCharm_01_twk at=35768,8763,5133":
            "The Golden Cat Bathhouse: Stairwell Room Bone Charm",
        cat + "DisAbstractItemPickup_20 DisAbstractItemPickup Rune_AbsItm at=35669,12837,4541":
            "The Golden Cat Bathhouse: Golden Cat Second Floor Rune",
        "additem DishonoredGameFull_P.TheWorld.PersistentLevel.Main_Sequence.fromStreets2_toHub."
        "DisSeqAct_AddAbstractItem_1 ChapterLog_Hub_03": "House of Pleasure: Victory",
    }
    for event, location in cases.items():
        assert game.match_event(event) == [ids[location]], event


def test_real_events_from_the_royal_physician():
    """Event lines from a playthrough of the fourth mission (2026-10-26)."""
    from dishonored import game_data

    ids = game_data.load().location_name_to_id
    game = NativeInterface()
    gate = "pickup L_Bridge_Part1a_Script.TheWorld.PersistentLevel."
    drawbridge = "pickup L_Bridge_Part1b_Script.TheWorld.PersistentLevel."
    midrow = "pickup L_Bridge_Part1c_Script.TheWorld.PersistentLevel."
    north = "pickup L_Bridge_Part2_Script.TheWorld.PersistentLevel."
    cases = {
        gate + "DisAbstractItemPickup_10 DisAbstractItemPickup BP_SilentRunning_AbsItm at=-3402,-5367,1404":
            "Southside Gate: Folded Galvani Resin Blueprint",
        gate + "DisAbstractItemPickup_40 DisAbstractItemPickup Rune_AbsItm at=-147,-1393,1357":
            "Southside Gate: Apartment Rune",
        gate + "DisWhaleBoneCharm_0 DisWhaleBoneCharm WhaleBoneCharm_01_twk at=5459,-2131,897":
            "Southside Gate: Past Metal Bridge Bone Charm",
        drawbridge + "DisAbstractItemPickup_3 DisAbstractItemPickup Rune_AbsItm at=5656,-3343,1555":
            "Drawbridge Control: Pratchett's Safe Rune",
        drawbridge + "DisAbstractItemPickup_4 DisAbstractItemPickup Rune_AbsItm at=7659,-3805,1850":
            "Drawbridge Control: Outsider Shrine Rune",
        midrow + "DisAbstractItemPickup_77 DisAbstractItemPickup Rune_AbsItm at=29324,-4671,1577":
            "Midrow Substation: Rune outside entry",
        midrow + "DisWhaleBoneCharm_2 DisWhaleBoneCharm WhaleBoneCharm_01_twk at=36183,-7161,437":
            "Midrow Substation: Rat Infested Basement Bone Charm",
        north + "DisWhaleBoneCharm_1 DisWhaleBoneCharm WhaleBoneCharm_01_twk at=38398,-8123,1490":
            "North End: Building By Entry Bone Charm",
        north + "DisAbstractItemPickup_0 DisAbstractItemPickup Rune_AbsItm at=42205,-3271,3112":
            "North End: Sokolov's Laboratory Rune",
        north + "DisAbstractItemPickup_1 DisAbstractItemPickup Coins_AbsItm at=43793,-1809,677":
            "North End: Sokolov's Safehouse Painting",
        "additem DishonoredGameFull_P.TheWorld.PersistentLevel.Main_Sequence.fromBridge_toHub."
        "DisSeqAct_AddAbstractItem_1 ChapterLog_Hub_04": "The Royal Physician: Victory",
        # In the pub after the mission; spawned at runtime, so matched by position.
        "pickup DishonoredGameFull_P.TheWorld.PersistentLevel.DisWhaleBoneCharm_5 DisWhaleBoneCharm "
        "WhaleBoneCharm_01_twk at=-1886,2977,457": "Hound Pits Pub: Abandoned Apartment Bone Charm",
    }
    for event, location in cases.items():
        assert game.match_event(event) == [ids[location]], event


def test_real_events_from_lady_boyles_last_party():
    """Event lines from a playthrough of the fifth mission (2026-10-27)."""
    from dishonored import game_data

    ids = game_data.load().location_name_to_id
    game = NativeInterface()
    ext = "pickup L_Boyle_Ext_Script.TheWorld.PersistentLevel."
    street = "pickup L_BoyleStreet_Ext_Script.TheWorld.PersistentLevel."
    interior = "pickup L_Boyle_Int_Script.TheWorld.PersistentLevel."
    spawned = "pickup DishonoredGameFull_P.TheWorld.PersistentLevel."
    cases = {
        "pickup L_Boyle_Ext_Audio.TheWorld.PersistentLevel.DisWhaleBoneCharm_0 DisWhaleBoneCharm "
        "WhaleBoneCharm_01_twk at=-12155,-3173,511": "Boyle Mansion Exterior: Outsider's Shrine Room Bone Charm",
        ext + "DisAbstractItemPickup_0 DisAbstractItemPickup Rune_AbsItm at=-11705,-2588,541":
            "Boyle Mansion Exterior: Outsider Shrine Rune",
        street + "DisAbstractItemPickup_68 DisAbstractItemPickup Rune_AbsItm at=-6768,-9050,-2565":
            "Boyle Mansion Exterior: Waterway Rune",
        street + "DisWhaleBoneCharm_1 DisWhaleBoneCharm WhaleBoneCharm_01_twk at=-3503,1967,311":
            "Boyle Mansion Exterior: Building by Mansion Bone Charm",
        street + "DisAbstractItemPickup_66 DisAbstractItemPickup BP_StickyGrenade_AbsItm at=-1573,-3975,-731":
            "Boyle Mansion Exterior: Spiked Grenade Housing Blueprint",
        interior + "DisAbstractItemPickup_66 DisAbstractItemPickup Coins_AbsItm at=1550,-463,993":
            "Boyle Mansion Interior: Display Room Painting",
        interior + "DisAbstractItemPickup_69 DisAbstractItemPickup Coins_AbsItm at=3770,-1992,1364":
            "Boyle Mansion Interior: Bedroom Painting",
        interior + "DisAbstractItemPickup_1 DisAbstractItemPickup Rune_AbsItm at=-831,-360,-1094":
            "Boyle Mansion Interior: Underground Vault Rune",
        "additem DishonoredGameFull_P.TheWorld.PersistentLevel.Main_Sequence.fromBoyle_toHub."
        "DisSeqAct_AddAbstractItem_1 ChapterLog_Hub_05": "Lady Boyle's Last Party: Victory",
        # In the pub after the mission.
        "pickup L_Pub_FromBoyle_Script.TheWorld.PersistentLevel.DisAbstractItemPickup_0 DisAbstractItemPickup "
        "Rune_AbsItm at=-1285,-1208,1426": "The Pub: Spare a Boyle Rune",
        # The rewards spawn where Emily hid (another run: at=-37,2258,116), so the action counts.
        "grant pickup Rune_twk by L_Pub_FromBoyle_Script.TheWorld.PersistentLevel.Main_Sequence."
        "DisSeqAct_GivePickup_0": "The Pub: Find Emily Rune",
        "grant pickup Rune_twk by L_Pub_FromBoyle_Script.TheWorld.PersistentLevel.Main_Sequence."
        "DisSeqAct_GivePickup_2": "The Pub: Reward for a duel with Shaw Rune",
        # Second run: another sister's bedroom.
        interior + "DisAbstractItemPickup_68 DisAbstractItemPickup Coins_AbsItm at=2396,3939,1071":
            "Boyle Mansion Interior: Bedroom Painting",
    }
    for event, location in cases.items():
        assert game.match_event(event) == [ids[location]], event
    assert game.match_event(spawned + "DisAbstractItemPickup_154 DisAbstractItemPickup Rune_AbsItm "
                            "at=-2684,-375,1439") == []
    # 5 s after entering the mansion its script destroys the other bedrooms' paintings from across
    # the house: the DLL tags that "(far)". Coming back in, the save removes them again while
    # loading. Neither counts.
    assert game.match_event(interior + "DisAbstractItemPickup_69 DisAbstractItemPickup Coins_AbsItm "
                            "at=3798,-2044,1189 (far)") == []
    assert game.match_event(interior + "DisAbstractItemPickup_69 DisAbstractItemPickup Coins_AbsItm "
                            "at=3798,-2044,1189 (load)") == []


def test_real_events_from_return_to_the_tower():
    """Event lines from a playthrough of the sixth mission (2026-10-27)."""
    from dishonored import game_data

    ids = game_data.load().location_name_to_id
    game = NativeInterface()
    yard = "pickup L_TowerRtrn_Yard_Script.TheWorld.PersistentLevel."
    inside = "pickup L_TowerRtrn_Int_Script.TheWorld.PersistentLevel."
    cases = {
        yard + "DisWhaleBoneCharm_0 DisWhaleBoneCharm WhaleBoneCharm_01_twk at=9803,23910,1362":
            "Tower Exterior: Waterlock Bone Charm",
        yard + "DisAbstractItemPickup_47 DisAbstractItemPickup Rune_AbsItm at=17976,27923,2945":
            "Return to the Tower: Bunkhouse Rune",
        inside + "DisAbstractItemPickup_6 DisAbstractItemPickup Rune_AbsItm at=-2972,10343,3910":
            "Return to the Tower: Torture Chamber Shrine Rune",
        inside + "DisAbstractItemPickup_3 DisAbstractItemPickup BP_ExpBullets_AbsItm at=-5842,7727,6005":
            "Tower Interior: Small-scale Combustion Refinement Blueprint",
        inside + "DisWhaleBoneCharm_0 DisWhaleBoneCharm WhaleBoneCharm_01_twk at=-1895,9146,6219":
            "Tower Interior: Jessamine's Secret Room Bone Charm",
        inside + "DisAbstractItemPickup_28 DisAbstractItemPickup Rune_AbsItm at=-8128,7214,6000":
            "Return to the Tower: Bedroom Rune",
        inside + "DisAbstractItemPickup_8 DisAbstractItemPickup Coins_AbsItm at=-8649,5938,5957":
            "Tower Interior: Piano Room Painting",
        yard + "DisAbstractItemPickup_6 DisAbstractItemPickup Rune_AbsItm at=29621,23988,6733":
            "Return to the Tower: Safehouse Rune",
        "additem DishonoredGameFull_P.TheWorld.PersistentLevel.Main_Sequence.fromTowerReturn_toHub."
        "DisSeqAct_AddAbstractItem_1 ChapterLog_Hub_06": "Return to the Tower: Victory",
    }
    for event, location in cases.items():
        assert game.match_event(event) == [ids[location]], event


def test_real_events_from_the_flooded_district():
    """Event lines from a playthrough of the seventh mission, up to Rudshore Gate (2026-10-29)."""
    from dishonored import game_data

    ids = game_data.load().location_name_to_id
    game = NativeInterface()
    streets = "pickup L_Flooded_FStreets_PickUps.TheWorld.PersistentLevel."
    refinery = "pickup L_Flooded_FRefinery_PickUps.TheWorld.PersistentLevel."
    base = "pickup L_Flooded_FAssassins_PickUps.TheWorld.PersistentLevel."
    sewer = "pickup L_Streetsewer_Script.TheWorld.PersistentLevel."
    cases = {
        streets + "DisWhaleBoneCharm_0 DisWhaleBoneCharm WhaleBoneCharm_01_twk at=-7268,5393,1779":
            "Rudshore Waterfront: End of Street Bone Charm",
        streets + "DisAbstractItemPickup_12 DisAbstractItemPickup Coins_AbsItm at=-9115,2267,1895":
            "Rudshore Waterfront: Weeper Room Painting",
        refinery + "DisAbstractItemPickup_81 DisAbstractItemPickup Rune_AbsItm at=4894,2166,67":
            "The Flooded District: Refinery Rune",
        refinery + "DisWhaleBoneCharm_0 DisWhaleBoneCharm WhaleBoneCharm_01_twk at=4186,7373,2062":
            "Greaves Refinery: Back of Tower Bone Charm",
        "objective L_Flooded_FRefinery_P.TheWorld.PersistentLevel.Main_Sequence.Objectives_Manager."
        "DisSeqAct_CompleteObjective_0 DisSeqAct_CompleteObjective Flooded_Gear": "Greaves Refinery: Retrieve your Gear",
        base + "DisWhaleBoneCharm_1 DisWhaleBoneCharm WhaleBoneCharm_01_twk at=-3587,5671,508":
            "Central Rudshore: Wolfhound Guarded Bone Charm",
        "grant pickup WhaleBoneCharm_01_twk by L_Flooded_FAssassins_Script.TheWorld.PersistentLevel.Main_Sequence."
        "Daud_Behavior.DisSeqAct_GivePickup_1": "Central Rudshore: Daud's Bone Charm",
        "pickup L_Flooded_FAssassins_Script.TheWorld.PersistentLevel.DisAbstractItemPickup_168 DisAbstractItemPickup "
        "Coins_AbsItm at=-2634,-38,285": "Central Rudshore: Daud's Base Exit Painting",
        base + "DisAbstractItemPickup_23 DisAbstractItemPickup Rune_AbsItm at=-1838,-1019,-972":
            "The Flooded District: Sewer Exit Rune",
        "pickup L_Flooded_FGate_Script.TheWorld.PersistentLevel.DisWhaleBoneCharm_0 DisWhaleBoneCharm "
        "WhaleBoneCharm_01_twk at=7043,2867,-1196": "Rudshore Gate: Zone Entry Bone Charm",
        "pickup L_Flooded_FGate_PickUps.TheWorld.PersistentLevel.DisAbstractItemPickup_3 DisAbstractItemPickup "
        "Rune_AbsItm at=-3801,4372,2995": "The Flooded District: Rudshore Gate Rooftop Rune",
        sewer + "DisWhaleBoneCharm_1 DisWhaleBoneCharm WhaleBoneCharm_01_twk at=404,4683,1877":
            "Old Dunwall Sewers: Collapsed Building Bone Charm",
        sewer + "DisWhaleBoneCharm_0 DisWhaleBoneCharm WhaleBoneCharm_01_twk at=-3865,250,-60":
            "Old Dunwall Sewers: Safe Bone Charm",
        sewer + "DisAbstractItemPickup_10 DisAbstractItemPickup Coins_AbsItm at=-8938,6391,571":
            "Old Dunwall Sewers: Granny Rags Painting",
        "pickup DishonoredGameFull_P.TheWorld.PersistentLevel.DisAbstractItemPickup_213 DisAbstractItemPickup "
        "Rune_AbsItm at=-6778,4192,1167": "The Flooded District: Granny's Outsider Shrine Rune",
        sewer + "DisAbstractItemPickup_16 DisAbstractItemPickup Rune_AbsItm at=6778,-3466,-924":
            "The Flooded District: Survivor Camp Rune",
        "pickup L_Streetsewer_Block.TheWorld.PersistentLevel.DisWhaleBoneCharm_1 DisWhaleBoneCharm "
        "WhaleBoneCharm_01_twk at=5691,1057,-871": "Old Dunwall Sewers: Past Survivor's Camp Bone Charm",
        "additem L_Pub_FromFlooded_Script.TheWorld.PersistentLevel.Main_Sequence.DisSeqAct_AddAbstractItem_1 "
        "ChapterLog_Loyalists": "The Flooded District: Victory",
    }
    for event, location in cases.items():
        assert game.match_event(event) == [ids[location]], event
    # Same number in the other package of the same level.
    assert game.match_event(base.replace("_PickUps", "_Script") + "DisAbstractItemPickup_23 "
                            "DisAbstractItemPickup Coins_AbsItm at=0,0,0") == []
