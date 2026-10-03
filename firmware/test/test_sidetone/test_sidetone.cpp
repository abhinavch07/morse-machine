// Unit tests for lib/sidetone. They run on the Mac with: pio test -e native

#include <math.h>
#include <stdlib.h>
#include <unity.h>

#include "sidetone.h"

const float RAMP_MS = 5;
const float SAMPLE_RATE = 40000;
const int RAMP_SAMPLES = 200;   // 5 ms at 40 kHz

const ToneShape ALL_SHAPES[] = {TONE_SOFT, TONE_BRIGHT, TONE_SHARP};

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

// Every table entry, added to 128, fits the DAC range 0 to 255, and the
// biggest entry is exactly 127, so the scaling uses the full range.
void test_table_fits_dac_range() {
  for (ToneShape shape : ALL_SHAPES) {
    SidetoneGenerator gen;
    gen.begin(600, SAMPLE_RATE, RAMP_MS, shape);
    int biggest = 0;
    for (int i = 0; i < SINE_TABLE_SIZE; i++) {
      int level = DAC_MID + gen.waveAt(i);
      TEST_ASSERT_TRUE(level >= 0 && level <= 255);
      if (abs(gen.waveAt(i)) > biggest) biggest = abs(gen.waveAt(i));
    }
    TEST_ASSERT_EQUAL_INT(127, biggest);
  }
}

// Measure how much of one harmonic is in the table, the same way a
// spectrum analyser would.
float harmonicSize(const SidetoneGenerator& gen, int harmonic) {
  float sum = 0;
  for (int i = 0; i < SINE_TABLE_SIZE; i++) {
    sum += gen.waveAt(i) * sinf(2 * (float)M_PI * harmonic * i / SINE_TABLE_SIZE);
  }
  return sum;
}

void test_presets_have_the_right_harmonics() {
  const float THIRD[] = {0.0f, 0.25f, 0.33f};
  const float FIFTH[] = {0.0f, 0.10f, 0.20f};
  for (int s = 0; s < 3; s++) {
    SidetoneGenerator gen;
    gen.begin(600, SAMPLE_RATE, RAMP_MS, ALL_SHAPES[s]);
    float first = harmonicSize(gen, 1);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, THIRD[s], harmonicSize(gen, 3) / first);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, FIFTH[s], harmonicSize(gen, 5) / first);
  }
}

void test_silent_is_mid_level() {
  for (ToneShape shape : ALL_SHAPES) {
    SidetoneGenerator gen;
    gen.begin(600, SAMPLE_RATE, RAMP_MS, shape);
    for (int i = 0; i < 1000; i++) {
      TEST_ASSERT_EQUAL_UINT8(DAC_MID, gen.nextSample(false));
    }
  }
}

void test_fade_in_starts_at_mid_level() {
  for (ToneShape shape : ALL_SHAPES) {
    SidetoneGenerator gen;
    gen.begin(600, SAMPLE_RATE, RAMP_MS, shape);
    // The very first sample is exactly the middle, and the first 0.5 ms
    // stays close to it.
    TEST_ASSERT_EQUAL_UINT8(DAC_MID, gen.nextSample(true));
    for (int i = 1; i < 20; i++) {
      TEST_ASSERT_INT_WITHIN(5, DAC_MID, gen.nextSample(true));
    }
    // After the 5 ms ramp, it swings almost the whole 0 to 255 range.
    for (int i = 20; i < RAMP_SAMPLES; i++) gen.nextSample(true);
    int lowest = 255, highest = 0;
    for (int i = 0; i < 1000; i++) {
      int v = gen.nextSample(true);
      if (v < lowest) lowest = v;
      if (v > highest) highest = v;
    }
    TEST_ASSERT_INT_WITHIN(3, 1, lowest);
    TEST_ASSERT_INT_WITHIN(3, 255, highest);
  }
}

void test_fade_out_ends_at_mid_level() {
  for (ToneShape shape : ALL_SHAPES) {
    SidetoneGenerator gen;
    gen.begin(600, SAMPLE_RATE, RAMP_MS, shape);
    for (int i = 0; i < 1000; i++) gen.nextSample(true);
    // Exactly 5 ms after release the output is back at the middle and stays.
    for (int i = 0; i < RAMP_SAMPLES - 1; i++) gen.nextSample(false);
    for (int i = 0; i < 1000; i++) {
      TEST_ASSERT_EQUAL_UINT8(DAC_MID, gen.nextSample(false));
    }
  }
}

void test_pitch_is_600_hz() {
  for (ToneShape shape : ALL_SHAPES) {
    SidetoneGenerator gen;
    gen.begin(600, SAMPLE_RATE, RAMP_MS, shape);
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
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_rise_shape);
  RUN_TEST(test_fall_shape);
  RUN_TEST(test_table_fits_dac_range);
  RUN_TEST(test_presets_have_the_right_harmonics);
  RUN_TEST(test_silent_is_mid_level);
  RUN_TEST(test_fade_in_starts_at_mid_level);
  RUN_TEST(test_fade_out_ends_at_mid_level);
  RUN_TEST(test_pitch_is_600_hz);
  return UNITY_END();
}
