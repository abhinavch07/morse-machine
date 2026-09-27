"""Tests for the random practice group maker."""

import random

from trainer.koch import lesson_characters
from trainer.groups import make_groups, character_weights, GROUP_SIZE


def test_newest_character_gets_more_weight():
    chars = ["K", "M", "U"]
    assert character_weights(chars, "U") == [1, 1, 2]


def test_groups_use_only_lesson_characters():
    chars = lesson_characters(5)
    newest = chars[-1]
    groups = make_groups(chars, newest, 20, 8, target_seconds=8,
                         rng=random.Random(0))
    assert len(groups) >= 1
    for group in groups:
        assert len(group) == GROUP_SIZE
        for char in group:
            assert char in chars


def test_groups_are_repeatable_with_a_seed():
    chars = lesson_characters(5)
    newest = chars[-1]
    first = make_groups(chars, newest, 20, 8, target_seconds=8,
                        rng=random.Random(42))
    second = make_groups(chars, newest, 20, 8, target_seconds=8,
                         rng=random.Random(42))
    assert first == second
