// Small Morse library for the ESP32, copying the rules of morse_core in Python.
// It has no Arduino code in it, so the unit tests can run on the Mac too.

#pragma once

// The five Morse durations, all in milliseconds. Same as Timing in timing.py.
struct MorseTiming {
  float dit;       // 1 unit
  float dah;       // 3 units
  float intraGap;  // gap inside a character, 1 unit
  float charGap;   // gap between characters, 3 units
  float wordGap;   // gap between words, 7 units
};

// Morse code for one character, written with "." and "-", for example
// "...-" for V. Lowercase works too. Returns nullptr if the character is not
// in the table. Same table as CHAR_TO_MORSE in codes.py, without prosigns.
const char* morseCode(char c);

// Length of one dit in ms: 1200 / WPM. Returns 0 if wpm is not positive.
float ditLengthMs(float wpm);

// All five durations for a speed, standard timing (no Farnsworth yet).
MorseTiming computeTiming(float wpm);

// Total time to send a text, plus one final word gap, like
// total_duration_ms() in timing.py. Runs of spaces count as one word gap,
// a trailing space changes nothing, and unknown characters are skipped.
float totalDurationMs(const char* text, float wpm);
