#include "morse.h"

#include <ctype.h>
#include <stddef.h>

namespace {

struct MorseEntry {
  char c;
  const char* code;
};

// ITU table. Keep it in step with morse_core/codes.py.
const MorseEntry MORSE_TABLE[] = {
  // Letters A to Z
  {'A', ".-"},   {'B', "-..."}, {'C', "-.-."}, {'D', "-.."},  {'E', "."},
  {'F', "..-."}, {'G', "--."},  {'H', "...."}, {'I', ".."},   {'J', ".---"},
  {'K', "-.-"},  {'L', ".-.."}, {'M', "--"},   {'N', "-."},   {'O', "---"},
  {'P', ".--."}, {'Q', "--.-"}, {'R', ".-."},  {'S', "..."},  {'T', "-"},
  {'U', "..-"},  {'V', "...-"}, {'W', ".--"},  {'X', "-..-"}, {'Y', "-.--"},
  {'Z', "--.."},
  // Figures 0 to 9
  {'0', "-----"}, {'1', ".----"}, {'2', "..---"}, {'3', "...--"},
  {'4', "....-"}, {'5', "....."}, {'6', "-...."}, {'7', "--..."},
  {'8', "---.."}, {'9', "----."},
  // Punctuation from the ASOC test, plus "/" for the Koch order
  {'.', ".-.-.-"},  // full stop
  {',', "--..--"},  // comma
  {';', "-.-.-."},  // semicolon
  {'=', "-...-"},   // break sign (same code as the prosign BT)
  {'-', "-....-"},  // hyphen
  {'?', "..--.."},  // question mark
  {'/', "-..-."},   // slash
};

// Time to send one code, with the 1 unit gaps between its symbols but no gap
// after the last symbol.
float codeDurationMs(const char* code, const MorseTiming& t) {
  float total = 0;
  for (int i = 0; code[i] != '\0'; i++) {
    if (i > 0) total += t.intraGap;
    total += (code[i] == '.') ? t.dit : t.dah;
  }
  return total;
}

}  // namespace

const char* morseCode(char c) {
  char upper = (char)toupper((unsigned char)c);
  for (const MorseEntry& entry : MORSE_TABLE) {
    if (entry.c == upper) return entry.code;
  }
  return nullptr;
}

float ditLengthMs(float wpm) {
  if (wpm <= 0) return 0;  // Python raises an error here, but C++ on the ESP32 has no exceptions
  return 1200.0f / wpm;
}

MorseTiming computeTiming(float wpm) {
  float unit = ditLengthMs(wpm);
  return {unit, 3 * unit, unit, 3 * unit, 7 * unit};
}

float totalDurationMs(const char* text, float wpm) {
  MorseTiming t = computeTiming(wpm);
  float total = 0;
  bool sentAny = false;
  bool spaceSeen = false;

  for (int i = 0; text[i] != '\0'; i++) {
    if (text[i] == ' ') {
      spaceSeen = true;
      continue;
    }
    const char* code = morseCode(text[i]);
    if (code == nullptr) continue;

    // The gap before a character depends on whether a space came before it.
    if (sentAny) total += spaceSeen ? t.wordGap : t.charGap;
    total += codeDurationMs(code, t);
    sentAny = true;
    spaceSeen = false;
  }

  if (!sentAny) return 0;
  return total + t.wordGap;
}
