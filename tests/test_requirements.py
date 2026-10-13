import pytest

from dishonored import requirements as req


class FakeState:
    def __init__(self, **counts: int) -> None:
        self.counts = {k.replace("_", " "): v for k, v in counts.items()}

    def count(self, item: str) -> int:
        return self.counts.get(item, 0)

    def count_from_list(self, items) -> int:
        return sum(self.count(i) for i in items)


CATEGORIES = {"Combat": ["Sword", "Bullets", "Bolts"]}
COUNTS = {"Sword": 2, "Bullets": 2, "Bolts": 2, "Blink": 2, "Key A": 1, "Key B": 1}


def rule(text):
    return req.compile_rule(req.parse(text), CATEGORIES, COUNTS)


@pytest.mark.parametrize("empty", [None, [], "", "   "])
def test_empty_requirement_is_always(empty):
    assert rule(empty) is None


def test_item_count():
    r = rule("|Blink:2|")
    assert not r(FakeState(Blink=1))
    assert r(FakeState(Blink=2))


def test_and_binds_tighter_than_or():
    r = rule("|Key A| OR |Key B| AND |Sword|")
    assert r(FakeState(Key_A=1))
    assert not r(FakeState(Key_B=1))
    assert r(FakeState(Key_B=1, Sword=1))


def test_parentheses():
    r = rule("(|Key A| OR |Key B|) and |Sword|")
    assert not r(FakeState(Key_A=1))
    assert r(FakeState(Key_B=1, Sword=1))


def test_category_counts():
    assert rule("|@Combat:2|")(FakeState(Sword=1, Bolts=1))
    assert not rule("|@Combat:2|")(FakeState(Sword=1))
    assert not rule("|@Combat:ALL|")(FakeState(Sword=2, Bullets=2, Bolts=1))
    assert rule("|@Combat:HALF|")(FakeState(Bolts=3))


def test_list_means_all():
    r = rule(["|Key A|", "|Key B|"])
    assert not r(FakeState(Key_A=1))
    assert r(FakeState(Key_A=1, Key_B=1))


@pytest.mark.parametrize("bad", ["|Key A| AND", "|Key A| |Key B|", "(|Key A|", "Key A", "|Blink:x|"])
def test_malformed(bad):
    with pytest.raises(req.RequirementError):
        rule(bad)


def test_unknown_names():
    with pytest.raises(req.RequirementError):
        rule("|Nope|")
    with pytest.raises(req.RequirementError):
        rule("|@Nope:1|")
