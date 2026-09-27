"""Make random practice groups for a Koch lesson.

Real Morse practice uses groups of five random characters. We fill about one
minute of sending time with such groups, drawn from the current lesson's
character set. The newest character gets a little more weight so you hear it
more often. This module has no sound. It only decides which characters to send.
"""

import random

from morse_core.timing import total_duration_ms

GROUP_SIZE = 5
NEW_CHAR_WEIGHT = 2  # the newest character is twice as likely as the others


def character_weights(chars, newest):
    """Return a weight for each character, higher for the newest one."""
    return [NEW_CHAR_WEIGHT if c == newest else 1 for c in chars]


def make_groups(chars, newest, char_wpm, effective_wpm=None,
                target_seconds=60, rng=None):
    """Build a list of five character groups lasting about target_seconds.

    We keep adding groups until sending them all would pass the target time,
    always keeping at least one group. Pass your own random generator (rng) to
    get repeatable results in tests.
    """
    rng = rng or random.Random()
    weights = character_weights(chars, newest)

    groups = []
    while True:
        group = "".join(rng.choices(chars, weights=weights, k=GROUP_SIZE))
        text = " ".join(groups + [group])
        seconds = total_duration_ms(text, char_wpm, effective_wpm) / 1000.0
        if groups and seconds > target_seconds:
            break
        groups.append(group)
    return groups
