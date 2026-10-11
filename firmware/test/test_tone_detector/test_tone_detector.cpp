// Unit tests for lib/tone_detector. They run on the Mac with: pio test -e native
// Same settings as main.cpp: 8000 samples a second, 10 ms blocks, fixed
// tone 600 Hz. Every sample has a DC level of 1550 ADC counts added, like
// the MAX9814 output at 1.25 V.

#include <unity.h>

#include <math.h>

#include "tone_detector.h"

const float RATE = 8000;
const int BLOCK = 80;
const float DC = 1550;
const float PI_F = 3.14159265f;

void setUp() {}
void tearDown() {}

unsigned long noiseState = 12345;
float noise() {
  noiseState = noiseState * 1103515245UL + 12345UL;
  return ((noiseState >> 16) & 0x7FFF) / 16383.5f - 1.0f;
}

// One piece of sound: ms long, at toneHz (0 for silence), with amplitude.
struct Piece {
  int ms;
  float toneHz;
  float amplitude;
};

// What the detector did: every change, with its time.
struct Result {
  int changes;
  bool on[64];
  unsigned long atMs[64];
};

// Plays the pieces one after the other, with background noise of the given
// amplitude, and records every change.
Result play(ToneDetector& d, const Piece* pieces, int count, float noiseAmplitude) {
  Result r = {};
  unsigned long sample = 0;
  for (int p = 0; p < count; p++) {
    int samples = pieces[p].ms * (int)RATE / 1000;
    for (int i = 0; i < samples; i++, sample++) {
      float x = DC + noiseAmplitude * noise();
      if (pieces[p].toneHz > 0) {
        x += pieces[p].amplitude * sinf(2 * PI_F * pieces[p].toneHz * sample / RATE);
      }
      if (d.addSample(x) && d.changed() && r.changes < 64) {
        r.on[r.changes] = d.toneOn();
        r.atMs[r.changes] = d.changeMs();
        r.changes++;
      }
    }
  }
  return r;
}

ToneDetector makeDetector() { return ToneDetector(RATE, BLOCK, 600); }

void test_block_is_10_ms() {
  ToneDetector d = makeDetector();
  for (int i = 0; i < BLOCK * 5; i++) d.addSample(DC);
  TEST_ASSERT_EQUAL(50, d.nowMs());
}

void test_silence_never_turns_on() {
  ToneDetector d = makeDetector();
  const Piece quiet[] = {{3000, 0, 0}};
  TEST_ASSERT_EQUAL(0, play(d, quiet, 1, 5).changes);
}

// A clean 600 Hz tone of 240 ms (a 15 WPM dah) after quiet. On and off once,
// at the right times, to within one block.
void test_clean_tone() {
  ToneDetector d = makeDetector();
  const Piece pieces[] = {{500, 0, 0}, {240, 600, 200}, {500, 0, 0}};
  Result r = play(d, pieces, 3, 2);
  TEST_ASSERT_EQUAL(2, r.changes);
  TEST_ASSERT_TRUE(r.on[0]);
  TEST_ASSERT_UINT32_WITHIN(10, 500, r.atMs[0]);
  TEST_ASSERT_FALSE(r.on[1]);
  TEST_ASSERT_UINT32_WITHIN(10, 740, r.atMs[1]);
}

// Noise half as loud as the tone: still exactly one on and one off, no
// flicker in the middle of the tone.
void test_tone_with_noise() {
  ToneDetector d = makeDetector();
  const Piece pieces[] = {{1000, 0, 0}, {300, 600, 200}, {1000, 0, 0}};
  Result r = play(d, pieces, 3, 100);
  TEST_ASSERT_EQUAL(2, r.changes);
  TEST_ASSERT_UINT32_WITHIN(20, 1000, r.atMs[0]);
  TEST_ASSERT_UINT32_WITHIN(20, 1300, r.atMs[1]);
}

// A loud tone between filter points (630 Hz) leaks a little into every
// filter. Through a run of 15 WPM dahs the noise level must stay far below
// the tone, and every dah must still be caught.
void test_loud_tone_does_not_raise_noise() {
  ToneDetector d = makeDetector();
  const Piece quiet[] = {{500, 0, 0}};
  play(d, quiet, 1, 2);
  const Piece dah[] = {{240, 630, 1000}, {80, 0, 0}};
  int changes = 0;
  for (int i = 0; i < 6; i++) {
    changes += play(d, dah, 1, 2).changes;
    TEST_ASSERT_TRUE(d.noise() < d.strength() * 0.1f);
    changes += play(d, dah + 1, 1, 2).changes;
  }
  TEST_ASSERT_EQUAL(12, changes);
}

// Fixed mode at 600 Hz ignores a loud 700 Hz tone, like the relay sidetone.
void test_wrong_frequency_ignored() {
  ToneDetector d = makeDetector();
  const Piece pieces[] = {{500, 0, 0}, {1000, 700, 200}, {500, 0, 0}};
  TEST_ASSERT_EQUAL(0, play(d, pieces, 3, 2).changes);
}

// The background gets 30 times louder in one instant, like a fan switched
// on. The noise level rises slowly on purpose, so there can be a few short
// false tones at first. Within half a second it must settle and stay quiet,
// and a real tone above the new noise is heard again.
void test_noise_level_adapts() {
  ToneDetector d = makeDetector();
  const Piece quiet[] = {{1000, 0, 0}};
  play(d, quiet, 1, 2);
  float quietNoise = d.noise();
  const Piece settle[] = {{500, 0, 0}};
  play(d, settle, 1, 60);
  const Piece loud[] = {{2500, 0, 0}};
  TEST_ASSERT_EQUAL(0, play(d, loud, 1, 60).changes);
  TEST_ASSERT_TRUE(d.noise() > quietNoise * 3);
  TEST_ASSERT_FALSE(d.toneOn());

  const Piece tone[] = {{240, 600, 300}, {300, 0, 0}};
  TEST_ASSERT_EQUAL(2, play(d, tone, 2, 60).changes);
}

void test_dits_at_15_wpm() {
  ToneDetector d = makeDetector();
  const Piece pieces[] = {{500, 0, 0}, {80, 600, 200}, {80, 0, 0},
                          {80, 600, 200}, {80, 0, 0},  {80, 600, 200}, {300, 0, 0}};
  Result r = play(d, pieces, 7, 5);
  TEST_ASSERT_EQUAL(6, r.changes);
  for (int i = 0; i < 6; i += 2) {
    unsigned long length = r.atMs[i + 1] - r.atMs[i];
    TEST_ASSERT_UINT32_WITHIN(20, 80, length);
  }
}

// While ignored (my own key is down), a tone is not detected.
void test_ignored_tone() {
  ToneDetector d = makeDetector();
  const Piece quiet[] = {{500, 0, 0}};
  play(d, quiet, 1, 2);
  d.setIgnored(true);
  const Piece tone[] = {{300, 600, 200}};
  TEST_ASSERT_EQUAL(0, play(d, tone, 1, 2).changes);
  d.setIgnored(false);
  const Piece after[] = {{300, 0, 0}, {200, 600, 200}};
  TEST_ASSERT_EQUAL(1, play(d, after, 2, 2).changes);
}

// Auto mode finds a 700 Hz sender and locks on to it.
void test_auto_mode_locks_on() {
  ToneDetector d = makeDetector();
  d.setAutoMode(true);
  const Piece pieces[] = {{500, 0, 0},   {240, 700, 200}, {80, 0, 0},
                          {240, 700, 200}, {80, 0, 0},    {240, 700, 200}, {300, 0, 0}};
  Result r = play(d, pieces, 7, 2);
  TEST_ASSERT_EQUAL_FLOAT(700, d.lockedHz());
  TEST_ASSERT_TRUE(r.changes >= 4);  // at least the later dahs are caught
  TEST_ASSERT_FALSE(d.toneOn());
}

// Auto mode between filter points: a 630 Hz sender locks on to 650 or 600.
void test_auto_mode_between_points() {
  ToneDetector d = makeDetector();
  d.setAutoMode(true);
  const Piece pieces[] = {{500, 0, 0}, {500, 630, 200}, {300, 0, 0}};
  play(d, pieces, 3, 2);
  TEST_ASSERT_TRUE(d.lockedHz() == 600 || d.lockedHz() == 650);
}

// Auto mode does not wander off to some random filter in plain noise.
void test_auto_mode_stays_put_in_noise() {
  ToneDetector d = makeDetector();
  d.setAutoMode(true);
  const Piece quiet[] = {{3000, 0, 0}};
  play(d, quiet, 1, 20);
  TEST_ASSERT_EQUAL_FLOAT(600, d.lockedHz());
}

// Back to fixed mode: straight back to 600 Hz.
void test_fixed_mode_returns_to_600() {
  ToneDetector d = makeDetector();
  d.setAutoMode(true);
  const Piece pieces[] = {{500, 0, 0}, {500, 800, 200}, {200, 0, 0}};
  play(d, pieces, 3, 2);
  TEST_ASSERT_EQUAL_FLOAT(800, d.lockedHz());
  d.setAutoMode(false);
  TEST_ASSERT_EQUAL_FLOAT(600, d.lockedHz());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_block_is_10_ms);
  RUN_TEST(test_silence_never_turns_on);
  RUN_TEST(test_clean_tone);
  RUN_TEST(test_tone_with_noise);
  RUN_TEST(test_loud_tone_does_not_raise_noise);
  RUN_TEST(test_wrong_frequency_ignored);
  RUN_TEST(test_noise_level_adapts);
  RUN_TEST(test_dits_at_15_wpm);
  RUN_TEST(test_ignored_tone);
  RUN_TEST(test_auto_mode_locks_on);
  RUN_TEST(test_auto_mode_between_points);
  RUN_TEST(test_auto_mode_stays_put_in_noise);
  RUN_TEST(test_fixed_mode_returns_to_600);
  return UNITY_END();
}
