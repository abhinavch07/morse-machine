"""Command line tool to make a Morse WAV file.

Example:
    python -m morse_core.make_wav "PARIS PARIS" --wpm 20 --effective 8 --out paris.wav

This writes a WAV you can play in any audio player to hear the Morse.
"""

import argparse

from .audio import DEFAULT_FREQ, text_to_wav


def main():
    parser = argparse.ArgumentParser(description="Make a Morse code WAV file.")
    parser.add_argument("text", help="the text to send, for example \"PARIS PARIS\"")
    parser.add_argument("--wpm", type=float, default=20,
                        help="character speed in words per minute (default 20)")
    parser.add_argument("--effective", type=float, default=None,
                        help="overall Farnsworth speed, slower than --wpm "
                             "(default: same as --wpm)")
    parser.add_argument("--freq", type=float, default=DEFAULT_FREQ,
                        help=f"sidetone pitch in Hz (default {DEFAULT_FREQ:g})")
    parser.add_argument("--out", default="morse.wav",
                        help="output file name (default morse.wav)")
    args = parser.parse_args()

    seconds = text_to_wav(args.out, args.text, args.wpm, args.effective, args.freq)
    print(f"Wrote {args.out}: {seconds:.2f} seconds at {args.wpm:g} WPM "
          f"(effective {args.effective if args.effective else args.wpm:g} WPM).")


if __name__ == "__main__":
    main()
