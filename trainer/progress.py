"""Save and load the trainer's progress.

Every practice round is stored in data/progress.json with the date, lesson,
speeds, score and which characters were missed. We also remember the lesson to
start from next time. Scoring 90 percent or more on your current lesson moves
you up. This module has no sound.
"""

import json
from datetime import date as date_class
from pathlib import Path

from .koch import MAX_LESSON

PASS_PERCENT = 90
PROGRESS_PATH = str(Path("data") / "progress.json")


def passed(percent):
    """True if a score is good enough to move to the next lesson."""
    return percent >= PASS_PERCENT


def load_progress(path=PROGRESS_PATH):
    """Load the progress file, or return a fresh empty one if it is missing."""
    file = Path(path)
    if not file.exists():
        return {"current_lesson": 1, "history": []}
    return json.loads(file.read_text())


def save_result(path, lesson, char_wpm, effective_wpm, percent, errors, date=None):
    """Add one round to the history and save. Returns the updated progress.

    If the score passes and the round was at or above the current lesson, the
    saved starting lesson moves up by one (never past the last lesson).
    """
    progress = load_progress(path)
    progress["history"].append({
        "date": date or date_class.today().isoformat(),
        "lesson": lesson,
        "char_wpm": char_wpm,
        "effective_wpm": effective_wpm,
        "percent": round(percent, 1),
        "errors": errors,
    })

    if passed(percent) and lesson >= progress["current_lesson"]:
        progress["current_lesson"] = min(lesson + 1, MAX_LESSON)

    file = Path(path)
    file.parent.mkdir(parents=True, exist_ok=True)
    file.write_text(json.dumps(progress, indent=2))
    return progress


def weakest_characters(progress, top=5):
    """Return the most missed characters as a list of (char, miss_count).

    Misses are added up across every round in the history and sorted worst
    first. Ties are broken alphabetically so the order is steady.
    """
    totals = {}
    for entry in progress.get("history", []):
        for char, count in entry.get("errors", {}).items():
            totals[char] = totals.get(char, 0) + count
    ranked = sorted(totals.items(), key=lambda item: (-item[1], item[0]))
    return ranked[:top]
