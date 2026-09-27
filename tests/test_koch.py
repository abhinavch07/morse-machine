"""Tests for the Koch lesson character sets."""

import pytest

from trainer.koch import KOCH_ORDER, MAX_LESSON, lesson_characters
from morse_core.translate import encode


def test_counts():
    assert len(KOCH_ORDER) == 41
    assert MAX_LESSON == 40


def test_lesson_1_is_two_characters():
    assert lesson_characters(1) == ["K", "M"]


def test_lesson_uses_first_n_plus_one():
    assert lesson_characters(5) == KOCH_ORDER[:6]
    assert len(lesson_characters(5)) == 6


def test_lesson_bounds_raise():
    with pytest.raises(ValueError):
        lesson_characters(0)
    with pytest.raises(ValueError):
        lesson_characters(MAX_LESSON + 1)


def test_every_koch_character_can_be_encoded():
    # If any character were missing from codes.py, this would raise.
    for char in KOCH_ORDER:
        assert encode(char) != ""
