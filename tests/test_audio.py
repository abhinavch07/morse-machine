"""Tests for the Morse audio and WAV output."""

import wave

from morse_core.timing import text_to_key_events
from morse_core.audio import events_to_samples, text_to_wav, SAMPLE_RATE


def test_samples_count_matches_event_total():
    # The number of samples should match the total event time times the rate.
    events = text_to_key_events("PARIS", 20)
    total_ms = sum(duration for _, duration in events)
    samples = events_to_samples(events)
    expected = round(total_ms / 1000.0 * SAMPLE_RATE)
    assert abs(len(samples) - expected) < 5  # tiny per event rounding only


def test_wav_length_matches_event_total(tmp_path):
    events = text_to_key_events("PARIS", 20)
    total_seconds = sum(duration for _, duration in events) / 1000.0

    out = tmp_path / "paris.wav"
    reported = text_to_wav(str(out), "PARIS", 20)

    with wave.open(str(out), "rb") as wav_file:
        frames = wav_file.getnframes()
        rate = wav_file.getframerate()
        channels = wav_file.getnchannels()
        width = wav_file.getsampwidth()

    file_seconds = frames / rate
    assert channels == 1        # mono
    assert width == 2           # 16-bit
    assert rate == SAMPLE_RATE
    assert file_seconds == reported
    assert abs(file_seconds - total_seconds) < 0.01


def test_silence_events_are_zero():
    # A single key up event should be pure silence.
    samples = events_to_samples([(False, 100)])
    assert set(samples) == {0}


def test_tone_starts_and_ends_quietly():
    # The raised cosine ramp means the first and last samples of a tone are
    # near zero, which is what removes the click.
    samples = events_to_samples([(True, 100)])
    assert abs(samples[0]) < 50
    assert abs(samples[-1]) < 50
    # Somewhere in the middle it should be loud.
    assert max(abs(s) for s in samples) > 10000
