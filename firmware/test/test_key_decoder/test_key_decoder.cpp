// Unit tests for lib/key_decoder. They run on the Mac with: pio test -e native
// The limits match main.cpp: a press of 160 ms or more is a dah, a gap of
// 250 ms or more ends the letter, and 2500 ms or more ends the word.
// Clean timings below use 15 WPM presses (dit 80 ms, dah 240 ms, gap inside
// a letter 80 ms), with a 450 ms letter gap and a 3000 ms word gap.

#include <string.h>
#include <unity.h>

#include "key_decoder.h"

const float DIT_DAH_SPLIT_MS = 160;
const float LETTER_GAP_MS = 250;
const float WORD_GAP_MS = 2500;

// Collected output, written the way main.cpp prints it: each letter with no
// code, and one space for a word gap.
char output[200];

void setUp() { output[0] = '\0'; }
void tearDown() {}

KeyDecoder makeDecoder() {
  return KeyDecoder(DIT_DAH_SPLIT_MS, LETTER_GAP_MS, WORD_GAP_MS);
}

void collect(const DecodeResult& r) {
  if (r.letterDone) strcat(output, r.letter);
  if (r.wordDone) strcat(output, " ");
}

// Feed timings that take turns: press, gap, press, gap, and so on. Each gap
// is given in one keyUp() call with its full length.
void feed(KeyDecoder& d, const float* ms, int count) {
  for (int i = 0; i < count; i++) {
    if (i % 2 == 0) {
      d.keyDown(ms[i]);
    } else {
      collect(d.keyUp(ms[i]));
    }
  }
}

void test_k() {
  KeyDecoder d = makeDecoder();
  const float ms[] = {240, 80, 80, 80, 240, 3000};
  feed(d, ms, 6);
  TEST_ASSERT_EQUAL_STRING("K ", output);
}

void test_vu() {
  KeyDecoder d = makeDecoder();
  const float ms[] = {80, 80, 80, 80, 80, 80, 240, 450,  // V ...- then letter gap
                      80, 80, 80, 80, 240, 3000};        // U ..- then word gap
  feed(d, ms, 14);
  TEST_ASSERT_EQUAL_STRING("VU ", output);
}

void test_ar_prosign() {
  KeyDecoder d = makeDecoder();
  const float ms[] = {80, 80, 240, 80, 80, 80, 240, 80, 80, 3000};  // .-.-.
  feed(d, ms, 10);
  TEST_ASSERT_EQUAL_STRING("<AR> ", output);
}

void test_word_gap_between_letters() {
  KeyDecoder d = makeDecoder();
  const float ms[] = {80, 3000,   // E, then a word gap
                      240, 3000}; // T, then a word gap
  feed(d, ms, 4);
  TEST_ASSERT_EQUAL_STRING("E T ", output);
}

void test_letter_gap_is_not_a_word_gap() {
  // 2499 ms is a long letter gap, but still not a word gap.
  KeyDecoder d = makeDecoder();
  const float ms[] = {80, 2499, 240, 2500};
  feed(d, ms, 4);
  TEST_ASSERT_EQUAL_STRING("ET ", output);
}

void test_uneven_timing() {
  // A human fist: dits from 60 to 100 ms, dahs from 200 to 300 ms, and gaps
  // that wander too. "CQ" is -.-. --.-
  KeyDecoder d = makeDecoder();
  const float ms[] = {
      300, 70, 60, 100, 200, 90, 100, 300,  // C, then a letter gap
      220, 60, 280, 75, 95, 85, 250, 3000,  // Q, then a word gap
  };
  feed(d, ms, 16);
  TEST_ASSERT_EQUAL_STRING("CQ ", output);
}

void test_my_hand_sos() {
  // The owner's real debug log from the board, 3 Oct 2026: two SOS groups.
  // Letter gaps of 1370 and 1458 ms are longer than many textbook word gaps,
  // so the word limit must sit between them and the 3822 ms pause.
  KeyDecoder d = makeDecoder();
  const float ms[] = {
      // First SOS
      80, 80, 80, 80, 80, 1370,       // S (not logged, a clean stand-in), gap
      266, 106, 229, 124, 325, 618,   // O, gap
      59, 65, 45, 58, 113, 3822,      // S, pause between the groups
      // Second SOS
      66, 89, 80, 108, 139, 1458,     // S, gap
      214, 110, 180, 136, 271, 427,   // O, gap
      48, 75, 47, 68, 77, 5000,       // S, then the line ends (not logged)
  };
  feed(d, ms, 36);
  // One space from the 3822 pause, none inside either SOS. The space at the
  // end is the final 5000 ms pause.
  TEST_ASSERT_EQUAL_STRING("SOS SOS ", output);
}

void test_limits_are_exact() {
  KeyDecoder d = makeDecoder();
  d.keyDown(159);  // just under the split: a dit
  d.keyUp(80);
  d.keyDown(160);  // exactly the split: a dah

  DecodeResult r = d.keyUp(249);  // just under the letter gap: nothing yet
  TEST_ASSERT_FALSE(r.letterDone);

  r = d.keyUp(250);  // exactly the letter gap: the letter is done
  TEST_ASSERT_TRUE(r.letterDone);
  TEST_ASSERT_FALSE(r.wordDone);
  TEST_ASSERT_EQUAL_STRING("A", r.letter);
  TEST_ASSERT_EQUAL_STRING(".-", r.code);
}

void test_letter_shows_while_key_is_still_up() {
  // Like main.cpp: keyUp() is called again and again as the gap grows.
  KeyDecoder d = makeDecoder();
  d.keyDown(240);  // one dah, T

  DecodeResult r = d.keyUp(250);  // the letter is done
  TEST_ASSERT_TRUE(r.letterDone);
  TEST_ASSERT_EQUAL_STRING("T", r.letter);

  r = d.keyUp(600);  // still up: the letter must not come again
  TEST_ASSERT_FALSE(r.letterDone);
  TEST_ASSERT_FALSE(r.wordDone);

  r = d.keyUp(2499);  // just under the word gap: nothing
  TEST_ASSERT_FALSE(r.wordDone);

  r = d.keyUp(2500);  // exactly the word gap: the word is done
  TEST_ASSERT_TRUE(r.wordDone);

  r = d.keyUp(6000);  // a long pause: no more output
  TEST_ASSERT_FALSE(r.letterDone);
  TEST_ASSERT_FALSE(r.wordDone);
}

void test_no_output_before_first_press() {
  KeyDecoder d = makeDecoder();
  DecodeResult r = d.keyUp(10000);
  TEST_ASSERT_FALSE(r.letterDone);
  TEST_ASSERT_FALSE(r.wordDone);
}

void test_unknown_code_is_star() {
  // Eight dits is the error sign, which is not in the table.
  KeyDecoder d = makeDecoder();
  for (int i = 0; i < 8; i++) {
    d.keyDown(80);
    d.keyUp(80);
  }
  DecodeResult r = d.keyUp(450);
  TEST_ASSERT_TRUE(r.letterDone);
  TEST_ASSERT_EQUAL_STRING("*", r.letter);
  TEST_ASSERT_EQUAL_STRING("........", r.code);
}

void test_too_many_presses_is_star() {
  // 12 dits is more than the buffer holds. It must not crash, and gives "*".
  KeyDecoder d = makeDecoder();
  for (int i = 0; i < 12; i++) {
    d.keyDown(80);
    d.keyUp(80);
  }
  DecodeResult r = d.keyUp(450);
  TEST_ASSERT_TRUE(r.letterDone);
  TEST_ASSERT_EQUAL_STRING("*", r.letter);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_k);
  RUN_TEST(test_vu);
  RUN_TEST(test_ar_prosign);
  RUN_TEST(test_word_gap_between_letters);
  RUN_TEST(test_letter_gap_is_not_a_word_gap);
  RUN_TEST(test_uneven_timing);
  RUN_TEST(test_my_hand_sos);
  RUN_TEST(test_limits_are_exact);
  RUN_TEST(test_letter_shows_while_key_is_still_up);
  RUN_TEST(test_no_output_before_first_press);
  RUN_TEST(test_unknown_code_is_star);
  RUN_TEST(test_too_many_presses_is_star);
  return UNITY_END();
}
