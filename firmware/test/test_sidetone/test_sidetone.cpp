// Unit tests for lib/sidetone. They run on the Mac with: pio test -e native

#include <unity.h>

#include "sidetone.h"

const float RAMP_MS = 5;
const float SAMPLE_RATE = 40000;
const int RAMP_SAMPLES = 200;   // 5 ms at 40 kHz

void setUp() {}
void tearDown() {}

void test_rise_shape() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, envelopeRise(0, RAMP_MS));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, envelopeRise(2.5f, RAMP_MS));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, envelopeRise(5, RAMP_MS));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, envelopeRise(8, RAMP_MS));  // stays full
}

void test_fall_shape() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, envelopeFall(0, RAMP_MS));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, envelopeFall(2.5f, RAMP_MS));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, envelopeFall(5, RAMP_MS));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, envelopeFall(8, RAMP_MS));  // stays silent
}

void test_silent_is_mid_level() {
  SidetoneGenerator gen;
  gen.begin(600, SAMPLE_RATE, RAMP_MS);
  for (int i = 0; i < 1000; i++) {
    TEST_ASSERT_EQUAL_UINT8(DAC_MID, gen.nextSample(false));
  }
}

void test_rise_is_gentle_then_full() {
  SidetoneGenerator gen;
  gen.begin(600, SAMPLE_RATE, RAMP_MS);
  // First 0.5 ms: the wave starts tiny, close to the middle.
  for (int i = 0; i < 20; i++) {
    int v = gen.nextSample(true);
    TEST_ASSERT_INT_WITHIN(5, DAC_MID, v);
  }
  // After the 5 ms ramp, it swings almost the whole 0 to 255 range.
  for (int i = 20; i < RAMP_SAMPLES; i++) gen.nextSample(true);
  int lowest = 255, highest = 0;
  for (int i = 0; i < 1000; i++) {
    int v = gen.nextSample(true);
    if (v < lowest) lowest = v;
    if (v > highest) highest = v;
  }
  TEST_ASSERT_INT_WITHIN(2, 1, lowest);
  TEST_ASSERT_INT_WITHIN(2, 255, highest);
}

void test_fall_ends_at_mid_level() {
  SidetoneGenerator gen;
  gen.begin(600, SAMPLE_RATE, RAMP_MS);
  for (int i = 0; i < 1000; i++) gen.nextSample(true);
  // Exactly 5 ms after release the output is back at the middle and stays.
  for (int i = 0; i < RAMP_SAMPLES - 1; i++) gen.nextSample(false);
  for (int i = 0; i < 1000; i++) {
    TEST_ASSERT_EQUAL_UINT8(DAC_MID, gen.nextSample(false));
  }
}

void test_pitch_is_600_hz() {
  SidetoneGenerator gen;
  gen.begin(600, SAMPLE_RATE, RAMP_MS);
  // One second of samples. Count how often the wave crosses the middle going up.
  int crossings = 0;
  int previous = gen.nextSample(true);
  for (int i = 1; i < (int)SAMPLE_RATE; i++) {
    int v = gen.nextSample(true);
    if (previous < DAC_MID && v >= DAC_MID) crossings++;
    previous = v;
  }
  TEST_ASSERT_INT_WITHIN(1, 600, crossings);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_rise_shape);
  RUN_TEST(test_fall_shape);
  RUN_TEST(test_silent_is_mid_level);
  RUN_TEST(test_rise_is_gentle_then_full);
  RUN_TEST(test_fall_ends_at_mid_level);
  RUN_TEST(test_pitch_is_600_hz);
  return UNITY_END();
}
