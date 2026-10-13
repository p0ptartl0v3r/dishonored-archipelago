"""Parser for Manual-style requirement strings.

The data files in ``data/`` keep the format used by the Manual apworld so the
two stay interchangeable. A requirement is either a list (all entries must
hold, an empty list means "always") or a string such as::

    |Progressive Blink:2|
    |@Combat:2|
    |Zone Key A| AND (|Item B| OR |Item C|)

``|Name|`` needs one copy of an item, ``|Name:N|`` needs N copies,
``|@Category:N|`` needs N items from a category (``ALL`` and ``HALF`` are
also accepted). ``AND``/``OR`` are case-insensitive; AND binds tighter.

This module deliberately has no Archipelago imports so it can be tested on
its own. Rules are evaluated against anything with ``count(item)`` and
``count_from_list(items)`` methods (see ``StateView``).
"""

from __future__ import annotations

import re
from collections.abc import Callable, Iterable, Mapping, Sequence
from dataclasses import dataclass
from typing import Protocol, Union


class StateView(Protocol):
    def count(self, item: str) -> int: ...

    def count_from_list(self, items: Iterable[str]) -> int: ...


Rule = Callable[[StateView], bool]


@dataclass(frozen=True)
class ItemReq:
    item: str
    count: int


@dataclass(frozen=True)
class CategoryReq:
    category: str
    count: str  # an int as text, "ALL" or "HALF"; resolved once categories are known


@dataclass(frozen=True)
class And:
    parts: tuple["Node", ...]


@dataclass(frozen=True)
class Or:
    parts: tuple["Node", ...]


@dataclass(frozen=True)
class Always:
    pass


Node = Union[ItemReq, CategoryReq, And, Or, Always]


class RequirementError(ValueError):
    pass


_TOKEN = re.compile(r"\s*(?:(\|[^|]+\|)|(\()|(\))|(AND\b)|(OR\b))", re.IGNORECASE)


def _tokenize(text: str) -> list[str]:
    tokens: list[str] = []
    pos = 0
    text = text.rstrip()
    while pos < len(text):
        match = _TOKEN.match(text, pos)
        if not match:
            raise RequirementError(f"Unexpected text at position {pos} in requirement: {text!r}")
        tokens.append(match.group(match.lastindex).strip())
        pos = match.end()
    return tokens


def _parse_term(token: str) -> Node:
    body = token[1:-1].strip()
    name, _, count = body.rpartition(":") if ":" in body else (body, "", "1")
    name, count = name.strip(), count.strip()
    if not name:
        raise RequirementError(f"Empty name in {token!r}")
    if name.startswith("@"):
        if not (count.isdigit() or count.upper() in ("ALL", "HALF")):
            raise RequirementError(f"Bad category count in {token!r}")
        return CategoryReq(name[1:], count.upper())
    if not count.isdigit():
        raise RequirementError(f"Bad item count in {token!r}")
    return ItemReq(name, int(count))


def parse(requires: Union[str, Sequence[str], None]) -> Node:
    """Parse a requirement from the data files into a tree."""
    if requires is None:
        return Always()
    if not isinstance(requires, str):
        parts = tuple(parse(r) for r in requires)
        if not parts:
            return Always()
        return parts[0] if len(parts) == 1 else And(parts)
    if not requires.strip():
        return Always()

    tokens = _tokenize(requires)
    pos = 0

    def peek() -> str | None:
        return tokens[pos] if pos < len(tokens) else None

    def take() -> str:
        nonlocal pos
        if pos >= len(tokens):
            raise RequirementError(f"Unexpected end of requirement: {requires!r}")
        pos += 1
        return tokens[pos - 1]

    def parse_or() -> Node:
        parts = [parse_and()]
        while (tok := peek()) is not None and tok.upper() == "OR":
            take()
            parts.append(parse_and())
        return parts[0] if len(parts) == 1 else Or(tuple(parts))

    def parse_and() -> Node:
        parts = [parse_atom()]
        while (tok := peek()) is not None and tok.upper() == "AND":
            take()
            parts.append(parse_atom())
        return parts[0] if len(parts) == 1 else And(tuple(parts))

    def parse_atom() -> Node:
        tok = take()
        if tok == "(":
            node = parse_or()
            if take() != ")":
                raise RequirementError(f"Missing ')' in requirement: {requires!r}")
            return node
        if tok.startswith("|"):
            return _parse_term(tok)
        raise RequirementError(f"Unexpected {tok!r} in requirement: {requires!r}")

    node = parse_or()
    if pos != len(tokens):
        raise RequirementError(f"Trailing {tokens[pos]!r} in requirement: {requires!r}")
    return node


def referenced_items(node: Node, categories: Mapping[str, Sequence[str]]) -> set[str]:
    """Every item name a requirement can depend on (categories expanded)."""
    if isinstance(node, ItemReq):
        return {node.item}
    if isinstance(node, CategoryReq):
        return set(categories.get(node.category, ()))
    if isinstance(node, (And, Or)):
        return set().union(*(referenced_items(p, categories) for p in node.parts))
    return set()


def compile_rule(
    node: Node,
    categories: Mapping[str, Sequence[str]],
    item_counts: Mapping[str, int],
) -> Rule | None:
    """Turn a parsed requirement into a callable. Returns None for "always"."""
    if isinstance(node, Always):
        return None
    if isinstance(node, ItemReq):
        if node.item not in item_counts:
            raise RequirementError(f"Unknown item {node.item!r}")
        item, count = node.item, node.count
        return lambda state: state.count(item) >= count
    if isinstance(node, CategoryReq):
        if node.category not in categories:
            raise RequirementError(f"Unknown category {node.category!r}")
        items = tuple(categories[node.category])
        total = sum(item_counts[i] for i in items)
        if node.count == "ALL":
            needed = total
        elif node.count == "HALF":
            needed = total // 2
        else:
            needed = int(node.count)
        return lambda state: state.count_from_list(items) >= needed
    sub_rules = [compile_rule(p, categories, item_counts) for p in node.parts]
    if isinstance(node, And):
        rules = [r for r in sub_rules if r is not None]
        if not rules:
            return None
        return lambda state: all(r(state) for r in rules)
    # Or: an "always" branch makes the whole thing always true.
    if any(r is None for r in sub_rules):
        return None
    return lambda state: any(r(state) for r in sub_rules)
