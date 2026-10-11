// Unit tests for lib/goertzel. They run on the Mac with: pio test -e native
// Same settings as main.cpp: 8000 samples a second, blocks of 80 (10 ms).
// The tones are made up here with sinf(), and the noise with a small random
// number maker that gives the same numbers every run.

#include <unity.h>

#include <math.h>

#include "goertzel.h"

const float RATE = 8000;
const int BLOCK = 80;
const float PI_F = 3.14159265f;

void setUp() {}
void tearDown() {}

// The same "random" numbers every run, from -1 to 1, so tests never flicker.
unsigned long noiseState = 12345;
float noise() {
  noiseState = noiseState * 1103515245UL + 12345UL;
  return ((noiseState >> 16) & 0x7FFF) / 16383.5f - 1.0f;
}

// Feed one block of a sine plus noise, and return the strength.
float measure(Goertzel& g, float toneHz, float amplitude, float noiseAmplitude) {
  for (int i = 0; i < BLOCK; i++) {
    float x = amplitude * sinf(2 * PI_F * toneHz * i / RATE) + noiseAmplitude * noise();
    if (g.addSample(x)) return g.strength();
  }
  return -1;  // never happens: one block is exactly BLOCK samples
}

void test_reports_once_per_block() {
  Goertzel g(600, RATE, BLOCK);
  int reports = 0;
  for (int i = 0; i < BLOCK * 3; i++) {
    if (g.addSample(0)) reports++;
  }
  TEST_ASSERT_EQUAL(3, reports);
}

// A clean 600 Hz sine of amplitude 100 measures 100.
void test_clean_tone() {
  Goertzel g(600, RATE, BLOCK);
  TEST_ASSERT_FLOAT_WITHIN(1, 100, measure(g, 600, 100, 0));
}

// Noise as loud as the tone itself still leaves the tone clearly measured.
void test_tone_with_noise() {
  Goertzel g(600, RATE, BLOCK);
  for (int block = 0; block < 20; block++) {
    TEST_ASSERT_FLOAT_WITHIN(25, 100, measure(g, 600, 100, 100));
  }
}

// The same noise alone measures far lower than the tone.
void test_noise_alone_is_low() {
  Goertzel g(600, RATE, BLOCK);
  for (int block = 0; block < 20; block++) {
    TEST_ASSERT_TRUE(measure(g, 600, 0, 100) < 30);
  }
}

// 1000 Hz is 4 filter widths away: almost nothing gets through.
void test_wrong_frequency() {
  Goertzel g(600, RATE, BLOCK);
  TEST_ASSERT_TRUE(measure(g, 1000, 100, 0) < 2);
}

// 850 Hz is between filter points, the worst case for leaking through.
// Still under a fifth of the real tone.
void test_wrong_frequency_between_points() {
  Goertzel g(600, RATE, BLOCK);
  TEST_ASSERT_TRUE(measure(g, 850, 100, 0) < 20);
}

// 700 Hz, the relay tone, is one filter width away and is rejected too.
void test_relay_tone_rejected() {
  Goertzel g(600, RATE, BLOCK);
  TEST_ASSERT_TRUE(measure(g, 700, 100, 0) < 2);
}

void test_silence() {
  Goertzel g(600, RATE, BLOCK);
  TEST_ASSERT_EQUAL_FLOAT(0, measure(g, 600, 0, 0));
}

// Each block starts fresh: a loud block does not leak into a silent one.
void test_blocks_are_independent() {
  Goertzel g(600, RATE, BLOCK);
  measure(g, 600, 100, 0);
  TEST_ASSERT_EQUAL_FLOAT(0, measure(g, 600, 0, 0));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_reports_once_per_block);
  RUN_TEST(test_clean_tone);
  RUN_TEST(test_tone_with_noise);
  RUN_TEST(test_noise_alone_is_low);
  RUN_TEST(test_wrong_frequency);
  RUN_TEST(test_wrong_frequency_between_points);
  RUN_TEST(test_relay_tone_rejected);
  RUN_TEST(test_silence);
  RUN_TEST(test_blocks_are_independent);
  return UNITY_END();
}
