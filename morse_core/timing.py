"""Morse timing engine.

This turns text into a list of key events. A key event is a tuple
(key_down, duration_ms). key_down True means the key is pressed and a tone
sounds. key_down False means silence (a gap).

All timing follows the spec in CLAUDE.md:
- 1 unit (a dit) in ms = 1200 / WPM
- dah = 3 units, gap inside a character = 1 unit
- gap between characters = 3 units, gap between words = 7 units
- Farnsworth: send characters fast (char_wpm) but stretch the gaps so the
  overall speed is slower (effective_wpm).

There is no audio here. This file only produces the on and off timing.
"""

from collections import namedtuple

from .translate import encode

# The five durations we need, all in milliseconds.
Timing = namedtuple("Timing", "dit dah intra_gap char_gap word_gap")


def dit_length_ms(wpm):
    """Return the length of one dit in milliseconds for a given WPM.

    This is the basic unit of Morse timing. Everything else is a multiple of it.
    """
    if wpm <= 0:
        raise ValueError(f"wpm must be positive, got {wpm}")
    return 1200.0 / wpm


def compute_timing(char_wpm, effective_wpm=None):
    """Work out the five Morse durations for a given speed.

    In standard timing the gaps are simple multiples of the dit length. In
    Farnsworth timing the characters stay at char_wpm speed but the character
    and word gaps are stretched so the whole message plays at effective_wpm.
    Asking for an effective speed faster than the character speed is an error.
    """
    unit = dit_length_ms(char_wpm)

    # Standard spacing: no Farnsworth, or the two speeds are the same.
    if effective_wpm is None or effective_wpm == char_wpm:
        return Timing(unit, 3 * unit, unit, 3 * unit, 7 * unit)

    if effective_wpm > char_wpm:
        raise ValueError(
            f"effective_wpm ({effective_wpm}) cannot be faster than char_wpm "
            f"({char_wpm}). Farnsworth only slows the spacing down."
        )

    # Farnsworth. ta is the total extra delay per word in seconds. It is shared
    # out as 3/19 into each character gap and 7/19 into each word gap.
    c, s = char_wpm, effective_wpm
    ta = (60 * c - 37.2 * s) / (s * c)
    char_gap = 3 * ta / 19 * 1000.0
    word_gap = 7 * ta / 19 * 1000.0
    return Timing(unit, 3 * unit, unit, char_gap, word_gap)


def text_to_key_events(text, char_wpm, effective_wpm=None):
    """Turn text into a list of (key_down, duration_ms) events.

    We reuse the Session 1 encoder to get the dots and dashes, then walk them:
    a dit or dah is a key_down event, and the gaps between symbols, characters
    and words are key_up events. Prosigns like <AR> encode to a single run of
    symbols, so they get no letter gaps inside, as intended. There is no
    trailing gap after the final symbol.
    """
    timing = compute_timing(char_wpm, effective_wpm)
    morse = encode(text)
    if morse == "":
        return []

    events = []
    words = morse.split(" / ")
    for word_index, word in enumerate(words):
        if word_index > 0:
            events.append((False, timing.word_gap))
        codes = word.split(" ")
        for code_index, code in enumerate(codes):
            if code_index > 0:
                events.append((False, timing.char_gap))
            for symbol_index, symbol in enumerate(code):
                if symbol_index > 0:
                    events.append((False, timing.intra_gap))
                if symbol == ".":
                    events.append((True, timing.dit))
                else:
                    events.append((True, timing.dah))
    return events


def total_duration_ms(text, char_wpm, effective_wpm=None):
    """Return the total time for a text including one final word gap.

    text_to_key_events leaves off the trailing gap so events can be joined
    cleanly. For speed maths we want the full slot a word occupies, so here we
    add one word gap at the end. For empty text the total is zero.
    """
    events = text_to_key_events(text, char_wpm, effective_wpm)
    if not events:
        return 0.0
    timing = compute_timing(char_wpm, effective_wpm)
    return sum(duration for _, duration in events) + timing.word_gap
