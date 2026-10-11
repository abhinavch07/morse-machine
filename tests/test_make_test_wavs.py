"""Tests for decoder/make_test_wavs.py."""

import wave

from morse_core.timing import total_duration_ms
from decoder.make_test_wavs import (
    LEAD_IN_MS, PITCHES_HZ, SPEEDS_WPM, TAIL_MS, TEXT, file_name, make_all,
)


def test_makes_one_file_per_speed_and_pitch(tmp_path):
    made = make_all(tmp_path)
    assert len(made) == len(SPEEDS_WPM) * len(PITCHES_HZ) == 6
    names = sorted(path.name for path, _ in made)
    assert "test_15wpm_600hz.wav" in names
    assert "test_20wpm_700hz.wav" in names


def test_lengths_match_the_text(tmp_path):
    for path, seconds in make_all(tmp_path):
        wpm = int(path.name.split("_")[1].replace("wpm", ""))
        # total_duration_ms adds one final word gap, the file has no gap but a tail.
        word_gap_ms = 7 * 1200 / wpm
        expected = (LEAD_IN_MS + total_duration_ms(TEXT, wpm) - word_gap_ms + TAIL_MS) / 1000
        assert abs(seconds - expected) < 0.01
        with wave.open(str(path), "rb") as wav_file:
            assert wav_file.getnchannels() == 1
            assert abs(wav_file.getnframes() / wav_file.getframerate() - seconds) < 0.001


def test_starts_with_silence(tmp_path):
    path, _ = make_all(tmp_path)[0]
    with wave.open(str(path), "rb") as wav_file:
        lead = wav_file.readframes(int(wav_file.getframerate() * LEAD_IN_MS / 1000))
    assert set(lead) == {0}


def test_file_name():
    assert file_name(10, 700) == "test_10wpm_700hz.wav"
