// Unit tests for lib/morse. They run on the Mac with: pio test -e native
// The numbers match the Python tests in tests/test_timing.py and
// tests/test_translate.py, so both versions follow the same rules.

#include <string.h>
#include <unity.h>

#include "morse.h"

void setUp() {}
void tearDown() {}

void test_dit_length() {
  TEST_ASSERT_EQUAL_FLOAT(60.0f, ditLengthMs(20));
  TEST_ASSERT_EQUAL_FLOAT(240.0f, ditLengthMs(5));
}

void test_bad_wpm_gives_zero() {
  TEST_ASSERT_EQUAL_FLOAT(0.0f, ditLengthMs(0));
}

void test_timing_multiples() {
  MorseTiming t = computeTiming(20);
  TEST_ASSERT_EQUAL_FLOAT(60.0f, t.dit);
  TEST_ASSERT_EQUAL_FLOAT(180.0f, t.dah);
  TEST_ASSERT_EQUAL_FLOAT(60.0f, t.intraGap);
  TEST_ASSERT_EQUAL_FLOAT(180.0f, t.charGap);
  TEST_ASSERT_EQUAL_FLOAT(420.0f, t.wordGap);
}

void test_v_and_u() {
  TEST_ASSERT_EQUAL_STRING("...-", morseCode('V'));
  TEST_ASSERT_EQUAL_STRING("..-", morseCode('U'));
}

void test_lowercase_figures_and_punctuation() {
  TEST_ASSERT_EQUAL_STRING("...-", morseCode('v'));
  TEST_ASSERT_EQUAL_STRING("-----", morseCode('0'));
  TEST_ASSERT_EQUAL_STRING("-...-", morseCode('='));
  TEST_ASSERT_EQUAL_STRING("-..-.", morseCode('/'));
}

void test_unknown_character() {
  TEST_ASSERT_NULL(morseCode('#'));
}

// Encode a whole token with morseCodeAt and check it used every char.
const char* encodeToken(const char* token) {
  int used = 0;
  const char* code = morseCodeAt(token, &used);
  TEST_ASSERT_EQUAL_INT((int)strlen(token), used);
  return code;
}

void test_each_prosign_encodes() {
  TEST_ASSERT_EQUAL_STRING(".-.-.", encodeToken("<AR>"));
  TEST_ASSERT_EQUAL_STRING("...-.-", encodeToken("<SK>"));
  TEST_ASSERT_EQUAL_STRING("-...-", encodeToken("<BT>"));
  TEST_ASSERT_EQUAL_STRING("-.--.", encodeToken("<KN>"));
  // Lowercase works, like Python, which uppercases the text first.
  TEST_ASSERT_EQUAL_STRING(".-.-.", encodeToken("<ar>"));
}

void test_prosigns_round_trip_except_shared_bt() {
  // <AR>, <SK> and <KN> have unique codes, so they round trip cleanly.
  TEST_ASSERT_EQUAL_STRING("<AR>", morseDecode(encodeToken("<AR>")));
  TEST_ASSERT_EQUAL_STRING("<SK>", morseDecode(encodeToken("<SK>")));
  TEST_ASSERT_EQUAL_STRING("<KN>", morseDecode(encodeToken("<KN>")));
  // <BT> and "=" share -...-, which decodes to the printable "=".
  TEST_ASSERT_EQUAL_STRING(morseCode('='), encodeToken("<BT>"));
  TEST_ASSERT_EQUAL_STRING("=", morseDecode("-...-"));
}

void test_decode_letters_and_unknown() {
  TEST_ASSERT_EQUAL_STRING("V", morseDecode("...-"));
  TEST_ASSERT_EQUAL_STRING("0", morseDecode("-----"));
  TEST_ASSERT_NULL(morseDecode("........"));
  TEST_ASSERT_NULL(morseDecode(""));
}

void test_bad_prosigns() {
  int used = 0;
  // Unknown prosign: the whole "<ZZ>" is skipped.
  TEST_ASSERT_NULL(morseCodeAt("<ZZ>", &used));
  TEST_ASSERT_EQUAL_INT(4, used);
  // No ">" in the same word: only the "<" is skipped.
  TEST_ASSERT_NULL(morseCodeAt("<AR", &used));
  TEST_ASSERT_EQUAL_INT(1, used);
  TEST_ASSERT_NULL(morseCodeAt("<AR B>", &used));
  TEST_ASSERT_EQUAL_INT(1, used);
}

void test_prosign_has_no_letter_gaps() {
  // <AR> is .-.-. sent as one run: 3 dits + 2 dahs + 4 gaps = 13 units,
  // plus a 7 unit word gap = 20 units = 1200 ms at 20 WPM. Python gives
  // total_duration_ms("<AR>", 20) == 1200 too.
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1200.0f, totalDurationMs("<AR>", 20));
  // Sent as two letters A R, a 3 unit letter gap adds up to 22 units.
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1320.0f, totalDurationMs("AR", 20));
  // A prosign as its own word after text, same as Python: 56 units.
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 3360.0f, totalDurationMs("CQ <KN>", 20));
}

void test_paris_is_50_units() {
  // PARIS plus one word gap is 50 units, and 50 x 60 ms is 3000 ms.
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 3000.0f, totalDurationMs("PARIS ", 20));
  // A trailing space must not change it, same as in Python.
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 3000.0f, totalDurationMs("PARIS", 20));
}

void test_empty_text() {
  TEST_ASSERT_EQUAL_FLOAT(0.0f, totalDurationMs("", 20));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_dit_length);
  RUN_TEST(test_bad_wpm_gives_zero);
  RUN_TEST(test_timing_multiples);
  RUN_TEST(test_v_and_u);
  RUN_TEST(test_lowercase_figures_and_punctuation);
  RUN_TEST(test_unknown_character);
  RUN_TEST(test_each_prosign_encodes);
  RUN_TEST(test_prosigns_round_trip_except_shared_bt);
  RUN_TEST(test_decode_letters_and_unknown);
  RUN_TEST(test_bad_prosigns);
  RUN_TEST(test_prosign_has_no_letter_gaps);
  RUN_TEST(test_paris_is_50_units);
  RUN_TEST(test_empty_text);
  return UNITY_END();
}
