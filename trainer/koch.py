"""The Koch character order for the trainer.

The Koch method teaches Morse by ear. You start with just two characters at
full speed and add one more each lesson. This file only stores the order and a
helper to get the character set for a lesson. It plays no sound.

The order below is the LCWO (Learn CW Online) order. Please check it against
the LCWO site before we build on it.
"""

# LCWO Koch order. Lesson 1 uses the first two, and each later lesson adds one.
# There are 41 characters, which gives 40 lessons.
KOCH_ORDER = [
    "K", "M", "U", "R", "E", "S", "N", "A", "P", "T",
    "L", "W", "I", ".", "J", "Z", "=", "F", "O", "Y",
    ",", "V", "G", "5", "/", "Q", "9", "2", "H", "3",
    "8", "B", "?", "4", "7", "C", "1", "D", "6", "0",
    "X",
]

# The highest lesson number available. Lesson 1 has 2 characters, so the last
# lesson number is len - 1.
MAX_LESSON = len(KOCH_ORDER) - 1


def lesson_characters(lesson):
    """Return the list of characters used in a given lesson.

    Lesson n uses the first n + 1 characters of the Koch order. So lesson 1 is
    the first 2 characters, lesson 2 the first 3, and so on. The lesson number
    must be between 1 and MAX_LESSON.
    """
    if not 1 <= lesson <= MAX_LESSON:
        raise ValueError(
            f"lesson must be between 1 and {MAX_LESSON}, got {lesson}"
        )
    return KOCH_ORDER[: lesson + 1]
