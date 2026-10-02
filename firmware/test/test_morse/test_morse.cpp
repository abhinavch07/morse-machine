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
  RUN_TEST(test_paris_is_50_units);
  RUN_TEST(test_empty_text);
  return UNITY_END();
}
