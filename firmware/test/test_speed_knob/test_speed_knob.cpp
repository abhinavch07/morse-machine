// Unit tests for lib/speed_knob. They run on the Mac with: pio test -e native
// Same settings as main.cpp: 8 to 25 WPM, ignore moves of 40 counts or less.

#include <unity.h>

#include "speed_knob.h"

void setUp() {}
void tearDown() {}

SpeedKnob makeKnob() { return SpeedKnob(8, 25, 40); }

// Feed the same reading many times, like a knob that is not moving.
// Returns how many times the WPM changed.
int feed(SpeedKnob& k, uint16_t raw, int count) {
  int changes = 0;
  for (int i = 0; i < count; i++) {
    if (k.update(raw)) changes++;
  }
  return changes;
}

void test_ends_of_the_turn() {
  SpeedKnob low = makeKnob();
  low.update(0);
  TEST_ASSERT_EQUAL(8, low.wpm());

  SpeedKnob high = makeKnob();
  high.update(4095);
  TEST_ASSERT_EQUAL(25, high.wpm());
}

// 4096 / 18 slices: 15 WPM is slice 7, from 7 x 227.6 = 1593 to 1820.
void test_middle_values() {
  SpeedKnob k = makeKnob();
  k.update(1700);
  TEST_ASSERT_EQUAL(15, k.wpm());

  SpeedKnob half = makeKnob();
  half.update(2048);
  TEST_ASSERT_EQUAL(17, half.wpm());
}

void test_first_reading_counts_as_a_change() {
  SpeedKnob k = makeKnob();
  TEST_ASSERT_TRUE(k.update(1700));
  TEST_ASSERT_EQUAL(1700, k.average());
}

// Noise around a slice edge (1593 is the 14 to 15 WPM edge) must not make
// the WPM flicker.
void test_jitter_is_ignored() {
  SpeedKnob k = makeKnob();
  k.update(1593);
  int startWpm = k.wpm();
  const uint16_t noisy[] = {1560, 1625, 1580, 1610, 1590, 1600, 1570, 1615};
  int changes = 0;
  for (int round = 0; round < 10; round++) {
    for (uint16_t raw : noisy) {
      if (k.update(raw)) changes++;
    }
  }
  TEST_ASSERT_EQUAL(0, changes);
  TEST_ASSERT_EQUAL(startWpm, k.wpm());
}

// One wild reading is the highest of the 16, so it is left out.
void test_single_spike_is_ignored() {
  SpeedKnob k = makeKnob();
  feed(k, 1700, 16);
  TEST_ASSERT_FALSE(k.update(4095));
  TEST_ASSERT_EQUAL(15, k.wpm());
}

// A real turn of the knob gets through once the average catches up. Going
// from 15 to 17 WPM passes 16 on the way, so that is 2 changes.
void test_real_turn_changes_wpm() {
  SpeedKnob k = makeKnob();
  feed(k, 1700, 16);
  TEST_ASSERT_EQUAL(2, feed(k, 2048, 16));
  TEST_ASSERT_EQUAL(17, k.wpm());
  TEST_ASSERT_TRUE(feed(k, 0, 16) > 0);
  TEST_ASSERT_EQUAL(8, k.wpm());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_ends_of_the_turn);
  RUN_TEST(test_middle_values);
  RUN_TEST(test_first_reading_counts_as_a_change);
  RUN_TEST(test_jitter_is_ignored);
  RUN_TEST(test_single_spike_is_ignored);
  RUN_TEST(test_real_turn_changes_wpm);
  return UNITY_END();
}
