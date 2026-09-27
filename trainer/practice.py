"""Run one Koch practice round in the terminal.

This ties the pieces together: pick a lesson, build about a minute of random
groups, play them as a sidetone through the macOS afplay command, then ask you
to type what you heard and score it. The audio is written to a temporary WAV
that is deleted straight after playing.

Example:
    python -m trainer.practice --lesson 3 --wpm 20 --effective 8
"""

import argparse
import os
import subprocess
import tempfile

from morse_core.audio import DEFAULT_FREQ, text_to_wav
from .koch import MAX_LESSON, lesson_characters
from .groups import make_groups
from .scoring import score
from .progress import (
    PROGRESS_PATH,
    load_progress,
    save_result,
    passed,
    weakest_characters,
)


def _play_wav(path):
    """Play a WAV file on macOS using afplay."""
    subprocess.run(["afplay", path], check=True)


def run_practice(lesson=None, char_wpm=20, effective_wpm=8,
                 progress_path=PROGRESS_PATH, freq=DEFAULT_FREQ):
    """Play one round, read the copy, score it and save the result."""
    progress = load_progress(progress_path)
    if lesson is None:
        lesson = progress["current_lesson"]

    chars = lesson_characters(lesson)
    newest = chars[-1]
    groups = make_groups(chars, newest, char_wpm, effective_wpm)
    sent = " ".join(groups)

    print(f"Lesson {lesson}: {' '.join(chars)}")
    print(f"{len(groups)} groups, about one minute. Newest character: {newest}")
    print("Listen, then type what you heard. Use - for a character you missed.\n")

    # Write the audio to a temp file, play it, then always delete the file.
    handle = tempfile.NamedTemporaryFile(suffix=".wav", delete=False)
    handle.close()
    try:
        text_to_wav(handle.name, sent, char_wpm, effective_wpm, freq)
        _play_wav(handle.name)
    finally:
        os.remove(handle.name)

    typed = input("You copied: ")
    result = score(sent, typed)

    print(f"\nSent:  {sent}")
    print(f"Score: {result.percent:.0f} percent "
          f"({result.length - result.distance} of {result.length} characters)")
    if result.wrong:
        pretty = ", ".join(f"{s or '_'} -> {t or '_'}" for s, t in result.wrong)
        print(f"Wrong: {pretty}")
    else:
        print("Clean copy, no mistakes.")

    save_result(progress_path, lesson, char_wpm, effective_wpm,
                result.percent, result.errors)

    if passed(result.percent):
        nxt = min(lesson + 1, MAX_LESSON)
        if nxt > lesson:
            print(f"\n90 percent or better. You can move up to lesson {nxt}.")
        else:
            print("\n90 percent or better on the final lesson. You have the full set.")
    else:
        print(f"\nUnder 90 percent. Worth repeating lesson {lesson}.")

    weak = weakest_characters(load_progress(progress_path), top=5)
    if weak:
        summary = ", ".join(f"{char} ({count})" for char, count in weak)
        print(f"Weakest so far: {summary}")


def main():
    parser = argparse.ArgumentParser(
        description="Koch Morse trainer, a one minute practice round."
    )
    parser.add_argument("--lesson", type=int, default=None,
                        help="lesson number (default: from saved progress)")
    parser.add_argument("--wpm", type=float, default=20,
                        help="character speed in WPM (default 20)")
    parser.add_argument("--effective", type=float, default=8,
                        help="effective Farnsworth speed in WPM (default 8)")
    parser.add_argument("--freq", type=float, default=DEFAULT_FREQ,
                        help=f"sidetone pitch in Hz (default {DEFAULT_FREQ:g})")
    args = parser.parse_args()
    run_practice(args.lesson, args.wpm, args.effective, freq=args.freq)


if __name__ == "__main__":
    main()
