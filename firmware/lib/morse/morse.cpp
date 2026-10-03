#include "morse.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

namespace {

// A token is one character like "A", or a prosign like "<AR>". Tokens are
// strings, not chars, so that prosigns fit in the same table.
struct MorseEntry {
  const char* token;
  const char* code;
};

// ITU table. Keep it in step with morse_core/codes.py, in the same order.
// The order matters for decoding: the first token with a code wins.
const MorseEntry MORSE_TABLE[] = {
  // Letters A to Z
  {"A", ".-"},   {"B", "-..."}, {"C", "-.-."}, {"D", "-.."},  {"E", "."},
  {"F", "..-."}, {"G", "--."},  {"H", "...."}, {"I", ".."},   {"J", ".---"},
  {"K", "-.-"},  {"L", ".-.."}, {"M", "--"},   {"N", "-."},   {"O", "---"},
  {"P", ".--."}, {"Q", "--.-"}, {"R", ".-."},  {"S", "..."},  {"T", "-"},
  {"U", "..-"},  {"V", "...-"}, {"W", ".--"},  {"X", "-..-"}, {"Y", "-.--"},
  {"Z", "--.."},
  // Figures 0 to 9
  {"0", "-----"}, {"1", ".----"}, {"2", "..---"}, {"3", "...--"},
  {"4", "....-"}, {"5", "....."}, {"6", "-...."}, {"7", "--..."},
  {"8", "---.."}, {"9", "----."},
  // Punctuation from the ASOC test, plus "/" for the Koch order
  {".", ".-.-.-"},  // full stop
  {",", "--..--"},  // comma
  {";", "-.-.-."},  // semicolon
  {"=", "-...-"},   // break sign (same code as <BT>)
  {"-", "-....-"},  // hyphen
  {"?", "..--.."},  // question mark
  {"/", "-..-."},   // slash
  // Prosigns. "=" is listed above <BT>, so -...- decodes to "=".
  {"<AR>", ".-.-."},   // end of message
  {"<SK>", "...-.-"},  // end of contact
  {"<BT>", "-...-"},   // break or new paragraph (same code as "=")
  {"<KN>", "-.--."},   // go ahead, named station only
};

// True if the first len chars of text match token exactly, ignoring case.
bool tokenMatches(const char* text, int len, const char* token) {
  if ((int)strlen(token) != len) return false;
  for (int i = 0; i < len; i++) {
    if (toupper((unsigned char)text[i]) != token[i]) return false;
  }
  return true;
}

const char* lookupToken(const char* text, int len) {
  for (const MorseEntry& entry : MORSE_TABLE) {
    if (tokenMatches(text, len, entry.token)) return entry.code;
  }
  return nullptr;
}

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
  return lookupToken(&c, 1);
}

const char* morseCodeAt(const char* text, int* used) {
  *used = 1;
  if (text[0] != '<') return lookupToken(text, 1);

  // A prosign ends at the first ">" in the same word, like in translate.py.
  int end = 1;
  while (text[end] != '\0' && text[end] != ' ' && text[end] != '>') end++;
  if (text[end] != '>') return nullptr;  // no ">": skip just the "<"

  *used = end + 1;
  return lookupToken(text, *used);
}

const char* morseDecode(const char* code) {
  for (const MorseEntry& entry : MORSE_TABLE) {
    if (strcmp(entry.code, code) == 0) return entry.token;
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

  int i = 0;
  while (text[i] != '\0') {
    if (text[i] == ' ') {
      spaceSeen = true;
      i++;
      continue;
    }
    int used;
    const char* code = morseCodeAt(&text[i], &used);
    i += used;
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
