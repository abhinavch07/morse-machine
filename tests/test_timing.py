"""Tests for the Morse timing engine."""

import pytest

from morse_core.timing import (
    dit_length_ms,
    compute_timing,
    text_to_key_events,
    total_duration_ms,
)


def test_dit_length():
    assert dit_length_ms(20) == 60
    assert dit_length_ms(8) == 150


def test_paris_standard_totals_3000ms():
    # PARIS plus one word space is the standard 50 units. At 20 WPM one unit is
    # 60 ms, so the whole slot is 3000 ms. A trailing space must not change it.
    assert total_duration_ms("PARIS", 20) == pytest.approx(3000)
    assert total_duration_ms("PARIS ", 20) == pytest.approx(3000)


def test_paris_farnsworth_totals_7500ms():
    # At an effective 8 WPM the same 50 unit slot lasts 50 x 150 ms = 7500 ms.
    assert total_duration_ms("PARIS", 20, 8) == pytest.approx(7500, abs=1)


def test_farnsworth_gaps():
    timing = compute_timing(20, 8)
    assert timing.char_gap == pytest.approx(891, abs=1)
    assert timing.word_gap == pytest.approx(2078, abs=1)
    # Characters are still sent at the fast speed, so a dit stays at 60 ms.
    assert timing.dit == pytest.approx(60)


def test_key_events_for_k():
    # K is dah dit dah. At 20 WPM: dah = 180 ms, gap inside a character = 60 ms,
    # dit = 60 ms. No trailing gap after the last dah.
    events = text_to_key_events("K", 20)
    assert events == [
        (True, 180),
        (False, 60),
        (True, 60),
        (False, 60),
        (True, 180),
    ]


def test_no_trailing_gap():
    events = text_to_key_events("E", 20)  # E is a single dit
    assert events == [(True, 60)]


def test_empty_text():
    assert text_to_key_events("", 20) == []
    assert total_duration_ms("", 20) == 0.0


def test_effective_faster_than_char_raises():
    with pytest.raises(ValueError):
        compute_timing(15, 20)
    with pytest.raises(ValueError):
        text_to_key_events("SOS", 15, 20)


def test_effective_equal_to_char_is_standard():
    # Passing the same speed for both must behave like standard timing.
    assert total_duration_ms("PARIS", 20, 20) == pytest.approx(3000)


def test_prosign_has_no_letter_gaps():
    # <AR> is .-.-. sent as one run: 3 dits + 2 dahs + 4 gaps = 13 units,
    # plus a 7 unit word gap = 20 units = 1200 ms at 20 WPM.
    assert total_duration_ms("<AR>", 20) == pytest.approx(1200)
    # Sent as two letters A R, a 3 unit letter gap adds up to 22 units.
    assert total_duration_ms("AR", 20) == pytest.approx(1320)
