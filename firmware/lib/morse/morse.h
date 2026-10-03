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
// in the table. Prosigns need more than one character, so use morseCodeAt()
// for those.
const char* morseCode(char c);

// Morse code for the token at the start of text. A token is one character,
// or a prosign in angle brackets like "<AR>", which is sent as one run with
// no letter gaps inside. Lowercase works too. *used is set to how many chars
// the token took (always at least 1), so the caller can step past it.
// Returns nullptr for an unknown character or prosign, or a "<" with no ">".
const char* morseCodeAt(const char* text, int* used);

// Text for one Morse code, for example "V" for "...-" or "<AR>" for ".-.-.".
// Same as MORSE_TO_CHAR in codes.py. The code -...- is shared by "=" and
// <BT>, and like Python it decodes to "=". Returns nullptr for an unknown code.
const char* morseDecode(const char* code);

// Length of one dit in ms: 1200 / WPM. Returns 0 if wpm is not positive.
float ditLengthMs(float wpm);

// All five durations for a speed, standard timing (no Farnsworth yet).
MorseTiming computeTiming(float wpm);

// Total time to send a text, plus one final word gap, like
// total_duration_ms() in timing.py. Runs of spaces count as one word gap,
// a trailing space changes nothing, and unknown characters are skipped.
// Prosigns like "<AR>" count as one character.
float totalDurationMs(const char* text, float wpm);
