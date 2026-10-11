"""Make test WAV files for the ESP32 microphone decoder.

Run it from the project folder, with the venv active:
    python -m decoder.make_test_wavs

It writes 6 files to decoder/test_wavs/, one for each speed (10, 15 and
20 WPM) and pitch (600 and 700 Hz), for example test_15wpm_600hz.wav. They
all send the same known text, so you can compare it with what the ESP32
decodes. Play one from the Mac near the microphone.

Each file starts with 2 seconds of silence, so the decoder can learn the
room's noise level before the first tone, then "VVV", the usual test call
that lets a listener lock on to the tone and speed. WAV files are not
committed to git (see .gitignore), so run this again on another computer.
"""

import argparse
from pathlib import Path

from morse_core.audio import events_to_samples, write_wav, SAMPLE_RATE
from morse_core.timing import text_to_key_events

TEXT = "VVV PARIS PARIS THE QUICK BROWN FOX 73"
SPEEDS_WPM = (10, 15, 20)
PITCHES_HZ = (600, 700)
LEAD_IN_MS = 2000      # silence before the first tone
TAIL_MS = 1000         # silence after the last tone, so the last word ends
OUT_DIR = Path(__file__).parent / "test_wavs"


def file_name(wpm, freq):
    return f"test_{wpm}wpm_{freq}hz.wav"


def make_wav(path, text, wpm, freq):
    """Write one test file. Returns its length in seconds."""
    events = [(False, LEAD_IN_MS)] + text_to_key_events(text, wpm) + [(False, TAIL_MS)]
    samples = events_to_samples(events, freq)
    write_wav(str(path), samples)
    return len(samples) / SAMPLE_RATE


def make_all(out_dir=OUT_DIR, text=TEXT):
    """Write all 6 files. Returns a list of (path, seconds)."""
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    made = []
    for wpm in SPEEDS_WPM:
        for freq in PITCHES_HZ:
            path = out_dir / file_name(wpm, freq)
            made.append((path, make_wav(path, text, wpm, freq)))
    return made


def main():
    parser = argparse.ArgumentParser(description="Make test WAV files for the mic decoder.")
    parser.add_argument("--out", default=str(OUT_DIR), help=f"folder for the files (default {OUT_DIR})")
    args = parser.parse_args()

    print(f'Text in every file: "{TEXT}"')
    for path, seconds in make_all(args.out):
        print(f"Wrote {path} ({seconds:.1f} seconds)")


if __name__ == "__main__":
    main()
