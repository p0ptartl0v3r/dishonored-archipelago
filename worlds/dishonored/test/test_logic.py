from . import DishonoredTestBase


class TestStart(DishonoredTestBase):
    def test_prologue_is_open(self) -> None:
        self.assertTrue(self.can_reach_location("Prologue: Hide And Seek"))

    def test_prison_needs_its_key(self) -> None:
        self.assertFalse(self.can_reach_region("Dunwall Prison"))
        self.collect_by_name("Dunwall Prison Zone Key")
        self.assertTrue(self.can_reach_region("Dunwall Prison"))

    def test_receive_the_mark_gives_blink(self) -> None:
        location = self.multiworld.get_location("The Void: Receive the Mark", self.player)
        self.assertEqual(location.item.name, "Progressive Blink")


class TestCategoryRule(DishonoredTestBase):
    def test_combat_items_count_towards_category_rules(self) -> None:
        # "|@Combat:2|" needs Progressive Sword to count even though items.json marks it only useful.
        item = self.get_item_by_name("Progressive Sword")
        self.assertTrue(item.advancement)


class TestGoal(DishonoredTestBase):
    def test_all_items_beat_the_game(self) -> None:
        self.collect_all_but([])
        self.assertBeatable(True)


def _count_in_pool(test: DishonoredTestBase, name: str) -> int:
    return sum(1 for item in test.multiworld.itempool if item.player == test.player and item.name == name)


class TestBoneCharmSlotsWithDLC(DishonoredTestBase):
    def test_ten_slots(self) -> None:
        self.assertEqual(_count_in_pool(self, "Bone Charm Slot"), 10)


class TestBoneCharmSlotsWithoutDLC(DishonoredTestBase):
    options = {"void_walkers_arsenal": False}

    def test_six_slots(self) -> None:
        # The game caps bone charm slots at 6 without the DLC's statues.
        self.assertEqual(_count_in_pool(self, "Bone Charm Slot"), 6)


class TestStartingZoneKeys(DishonoredTestBase):
    options = {"starting_zone_keys": "intro"}

    def test_intro_keys_are_given_and_not_shuffled(self) -> None:
        start = [item.name for item in self.multiworld.precollected_items[self.player]]
        for key in ("Dunwall Prison Zone Key", "Dunwall Sewers Zone Key", "Hound Pits Pub Zone Key", "The Void Zone Key"):
            self.assertIn(key, start)
            self.assertEqual(_count_in_pool(self, key), 0)
        self.assertTrue(self.can_reach_region("The Void"))
        self.assertEqual(_count_in_pool(self, "Distillery District (HOC) Zone Key"), 1)


class TestNoStartingZoneKeys(DishonoredTestBase):
    def test_prison_key_is_shuffled(self) -> None:
        self.assertEqual(self.multiworld.precollected_items[self.player], [])
        self.assertEqual(_count_in_pool(self, "Dunwall Prison Zone Key"), 1)


class TestSouthsideGateNeedsItsKey(DishonoredTestBase):
    def test_southside_gate_zone_key_is_required(self) -> None:
        # The Manual didn't require it; the in-game check enforces every zone's key.
        self.collect_all_but(["Southside Gate Zone Key"])
        self.assertFalse(self.can_reach_region("Southside Gate"))
        self.collect_by_name("Southside Gate Zone Key")
        self.assertTrue(self.can_reach_region("Southside Gate"))
