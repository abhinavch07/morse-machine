// Unit tests for lib/mic_decoder (SpeedTracker and MicDecoder). They run on
// the Mac with: pio test -e native
// The tone times are made from text with the Morse library, at standard
// timing, like the test WAV files. Every time is rounded to whole 10 ms
// blocks, as the tone detector reports them.

#include <unity.h>

#include <string.h>

#include <morse.h>

#include "mic_decoder.h"
#include "speed_tracker.h"

void setUp() {}
void tearDown() {}

// Round to the 10 ms blocks the tone detector works in.
unsigned long toBlocks(float ms) { return (unsigned long)(ms / 10 + 0.5f) * 10; }

// Decoded text, built up as letters and word gaps come out.
struct Text {
  char s[128];
  void add(const DecodeResult& r) {
    if (r.letterDone) strncat(s, r.letter, sizeof(s) - strlen(s) - 1);
    if (r.wordDone) strncat(s, " ", sizeof(s) - strlen(s) - 1);
  }
};

// Plays text at wpm into the decoder, starting at *t ms, and adds the
// decoded result to out. Calls update() every 10 ms in the gaps, like
// main.cpp does on every block.
void send(MicDecoder& d, const char* text, float wpm, unsigned long* t, Text* out) {
  MorseTiming timing = computeTiming(wpm);
  bool first = true;
  bool spaceSeen = false;
  for (int i = 0; text[i] != '\0'; i++) {
    if (text[i] == ' ') {
      spaceSeen = true;
      continue;
    }
    const char* code = morseCode(text[i]);
    if (!first) {
      unsigned long gapEnd = *t + toBlocks(spaceSeen ? timing.wordGap : timing.charGap);
      for (; *t < gapEnd; *t += 10) out->add(d.update(*t));
    }
    for (int k = 0; code[k] != '\0'; k++) {
      if (k > 0) *t += toBlocks(timing.intraGap);
      out->add(d.toneChanged(true, *t));
      *t += toBlocks(code[k] == '.' ? timing.dit : timing.dah);
      out->add(d.toneChanged(false, *t));
    }
    first = false;
    spaceSeen = false;
  }
}

// A long quiet gap at the end, to finish the last letter and word.
void finish(MicDecoder& d, unsigned long* t, Text* out) {
  for (int i = 0; i < 300; i++, *t += 10) out->add(d.update(*t));
}

void test_tracker_15_wpm() {
  SpeedTracker s(120);  // starts thinking 10 WPM
  const float tones[] = {80, 240, 240, 80, 80, 240, 80, 80};  // P A
  for (float ms : tones) s.addTone(ms);
  TEST_ASSERT_FLOAT_WITHIN(1, 80, s.ditMs());
  TEST_ASSERT_FLOAT_WITHIN(0.3, 15, s.wpm());
}

void test_tracker_all_dits() {
  SpeedTracker s(80);
  for (int i = 0; i < 8; i++) s.addTone(60);  // EEEE at 20 WPM
  TEST_ASSERT_FLOAT_WITHIN(1, 60, s.ditMs());
}

void test_tracker_all_dahs() {
  SpeedTracker s(80);
  for (int i = 0; i < 8; i++) s.addTone(180);  // TTTT at 20 WPM
  TEST_ASSERT_FLOAT_WITHIN(1, 60, s.ditMs());
}

// Uneven, like a human hand: dits 70 to 95, dahs 220 to 270.
void test_tracker_uneven_hand() {
  SpeedTracker s(80);
  const float tones[] = {70, 250, 95, 220, 85, 270, 75, 240};
  for (float ms : tones) s.addTone(ms);
  TEST_ASSERT_FLOAT_WITHIN(5, 80, s.ditMs());
}

void test_decode_15_wpm() {
  MicDecoder d(15);
  Text out = {};
  unsigned long t = 1000;
  send(d, "PARIS PARIS", 15, &t, &out);
  finish(d, &t, &out);
  TEST_ASSERT_EQUAL_STRING("PARIS PARIS ", out.s);
  TEST_ASSERT_FLOAT_WITHIN(0.5, 15, d.wpm());
}

// Starts thinking 15 WPM, but the sender is at 10 or 20: still right from
// the first letter, because these are within the 2 x split.
void test_decode_10_and_20_wpm() {
  MicDecoder slow(15);
  Text out = {};
  unsigned long t = 1000;
  send(slow, "CQ DE VU", 10, &t, &out);
  finish(slow, &t, &out);
  TEST_ASSERT_EQUAL_STRING("CQ DE VU ", out.s);
  TEST_ASSERT_FLOAT_WITHIN(0.5, 10, slow.wpm());

  MicDecoder fast(15);
  Text out2 = {};
  t = 1000;
  send(fast, "CQ DE VU", 20, &t, &out2);
  finish(fast, &t, &out2);
  TEST_ASSERT_EQUAL_STRING("CQ DE VU ", out2.s);
  TEST_ASSERT_FLOAT_WITHIN(1, 20, fast.wpm());
}

// A new sender much faster, 10 WPM then 25 WPM with no pause: a few letters
// may be wrong while it catches up, but then the speed and the text are
// right.
void test_adapts_to_speed_jump() {
  MicDecoder d(15);
  Text out = {};
  unsigned long t = 1000;
  send(d, "PARIS", 10, &t, &out);
  TEST_ASSERT_FLOAT_WITHIN(0.5, 10, d.wpm());
  t += 1000;
  send(d, "PARIS PARIS", 25, &t, &out);
  TEST_ASSERT_FLOAT_WITHIN(1.5, 25, d.wpm());

  finish(d, &t, &out);  // the pause before, run through update()
  Text last = {};
  send(d, "THE QUICK FOX", 25, &t, &last);
  finish(d, &t, &last);
  TEST_ASSERT_EQUAL_STRING("THE QUICK FOX ", last.s);
}

// And back down, 25 to 10 WPM.
void test_adapts_to_speed_drop() {
  MicDecoder d(15);
  Text out = {};
  unsigned long t = 1000;
  send(d, "PARIS PARIS", 25, &t, &out);
  t += 2000;
  send(d, "PARIS PARIS", 10, &t, &out);
  TEST_ASSERT_FLOAT_WITHIN(0.5, 10, d.wpm());

  finish(d, &t, &out);
  Text last = {};
  send(d, "73 DE VU", 10, &t, &last);
  finish(d, &t, &last);
  TEST_ASSERT_EQUAL_STRING("73 DE VU ", last.s);
}

void test_nothing_before_first_tone() {
  MicDecoder d(15);
  DecodeResult r = d.update(5000);
  TEST_ASSERT_FALSE(r.letterDone);
  TEST_ASSERT_FALSE(r.wordDone);
}

// After reset, half a letter from before does not join the next letter.
void test_reset_drops_half_letter() {
  MicDecoder d(15);
  Text out = {};
  d.toneChanged(true, 1000);
  d.toneChanged(false, 1080);  // one dit, then reset
  d.reset();
  unsigned long t = 2000;
  send(d, "T", 15, &t, &out);
  finish(d, &t, &out);
  TEST_ASSERT_EQUAL_STRING("T ", out.s);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_tracker_15_wpm);
  RUN_TEST(test_tracker_all_dits);
  RUN_TEST(test_tracker_all_dahs);
  RUN_TEST(test_tracker_uneven_hand);
  RUN_TEST(test_decode_15_wpm);
  RUN_TEST(test_decode_10_and_20_wpm);
  RUN_TEST(test_adapts_to_speed_jump);
  RUN_TEST(test_adapts_to_speed_drop);
  RUN_TEST(test_nothing_before_first_tone);
  RUN_TEST(test_reset_drops_half_letter);
  return UNITY_END();
}
