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
