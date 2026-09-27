"""Turn key events into a sidetone and save it as a WAV file.

We build 16-bit mono audio by hand using only the standard library (math,
array and wave), so there is no extra dependency to install. Each key_down
event becomes a sine tone, each key_up event becomes silence.

To stop the sharp clicks you would otherwise hear, every tone fades in and out
over 5 ms using a raised cosine shape (smooth, like the top half of a hill).
"""

import math
import wave
from array import array

from .timing import text_to_key_events

SAMPLE_RATE = 44100        # samples per second (CD quality)
DEFAULT_FREQ = 600.0       # sidetone pitch in Hz, from the CLAUDE.md spec
RAMP_MS = 5.0              # fade in and out time to avoid clicks
AMPLITUDE = 0.7           # fraction of full loudness, leaves headroom
_MAX_INT16 = 32767         # loudest value a 16-bit sample can hold


def events_to_samples(events, freq=DEFAULT_FREQ, sample_rate=SAMPLE_RATE):
    """Convert key events into a list of 16-bit samples.

    A key_down event is a sine wave at the given frequency with a smooth fade
    in and fade out. A key_up event is the same length of silence. If a tone is
    shorter than two ramps, the ramp is shortened so it still fits.
    """
    samples = array("h")  # "h" means signed 16-bit integers
    full_ramp = int(round(RAMP_MS / 1000.0 * sample_rate))
    peak = AMPLITUDE * _MAX_INT16

    for key_down, duration_ms in events:
        count = int(round(duration_ms / 1000.0 * sample_rate))
        if not key_down:
            samples.extend([0] * count)
            continue

        ramp = min(full_ramp, count // 2)
        for i in range(count):
            if ramp > 0 and i < ramp:
                # Rising edge: 0 up to 1 following a raised cosine.
                envelope = 0.5 * (1 - math.cos(math.pi * i / ramp))
            elif ramp > 0 and i >= count - ramp:
                # Falling edge: 1 down to 0, mirror of the rise.
                back = count - 1 - i
                envelope = 0.5 * (1 - math.cos(math.pi * back / ramp))
            else:
                envelope = 1.0
            value = envelope * peak * math.sin(2 * math.pi * freq * i / sample_rate)
            samples.append(int(value))

    return samples


def write_wav(path, samples, sample_rate=SAMPLE_RATE):
    """Write 16-bit mono samples to a WAV file using the wave module."""
    with wave.open(path, "wb") as wav_file:
        wav_file.setnchannels(1)      # mono
        wav_file.setsampwidth(2)      # 2 bytes = 16 bits per sample
        wav_file.setframerate(sample_rate)
        wav_file.writeframes(samples.tobytes())


def text_to_wav(path, text, char_wpm, effective_wpm=None,
                freq=DEFAULT_FREQ, sample_rate=SAMPLE_RATE):
    """Encode text to Morse audio and save it. Returns the length in seconds."""
    events = text_to_key_events(text, char_wpm, effective_wpm)
    samples = events_to_samples(events, freq, sample_rate)
    write_wav(path, samples, sample_rate)
    return len(samples) / sample_rate
