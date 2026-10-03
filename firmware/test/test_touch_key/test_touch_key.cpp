// Unit tests for lib/touch_key. They run on the Mac with: pio test -e native
// The limits match main.cpp: touched below 65% of the no touch level,
// released above 80%, and 3 readings in a row must agree.
// With a no touch level of 100, that means touched below 65, released above 80.

#include <unity.h>

#include "touch_key.h"

const float PRESS_RATIO = 0.65;
const float RELEASE_RATIO = 0.80;
const int READINGS_TO_AGREE = 3;

void setUp() {}
void tearDown() {}

// A key calibrated to a no touch level of exactly 100.
TouchKey makeKey() {
  TouchKey k(PRESS_RATIO, RELEASE_RATIO, READINGS_TO_AGREE);
  k.addCalibrationReading(100);
  k.finishCalibration();
  return k;
}

// Feeds readings 3 ms apart, starting at startMs, like main.cpp does.
// Returns how many times the state changed.
int feed(TouchKey& k, const uint16_t* raw, int count, unsigned long startMs = 0) {
  int changes = 0;
  for (int i = 0; i < count; i++) {
    if (k.update(raw[i], startMs + i * 3)) changes++;
  }
  return changes;
}

void test_calibration_is_the_average() {
  TouchKey k(PRESS_RATIO, RELEASE_RATIO, READINGS_TO_AGREE);
  const uint16_t raw[] = {48, 52, 50, 49, 51};
  for (int i = 0; i < 5; i++) k.addCalibrationReading(raw[i]);
  k.finishCalibration();
  TEST_ASSERT_EQUAL_FLOAT(50, k.noTouchLevel());
  TEST_ASSERT_EQUAL_FLOAT(32.5, k.pressLevel());
  TEST_ASSERT_EQUAL_FLOAT(40, k.releaseLevel());
}

void test_not_calibrated_never_touches() {
  TouchKey k(PRESS_RATIO, RELEASE_RATIO, READINGS_TO_AGREE);
  k.finishCalibration();  // no readings at all
  const uint16_t raw[] = {0, 0, 0, 0};
  TEST_ASSERT_EQUAL(0, feed(k, raw, 4));
  TEST_ASSERT_FALSE(k.touched());
}

void test_press_below_65_percent() {
  TouchKey k = makeKey();
  const uint16_t notLowEnough[] = {66, 65, 66, 65, 70};  // 65 is not below 65
  TEST_ASSERT_EQUAL(0, feed(k, notLowEnough, 5));
  TEST_ASSERT_FALSE(k.touched());

  const uint16_t touch[] = {40, 38, 39};
  TEST_ASSERT_EQUAL(1, feed(k, touch, 3, 100));
  TEST_ASSERT_TRUE(k.touched());
}

void test_no_release_until_above_80_percent() {
  TouchKey k = makeKey();
  const uint16_t touch[] = {40, 40, 40};
  feed(k, touch, 3);

  // Back above the press limit, but not above the release limit: still touched.
  const uint16_t between[] = {70, 75, 80, 78, 80, 72};
  TEST_ASSERT_EQUAL(0, feed(k, between, 6));
  TEST_ASSERT_TRUE(k.touched());

  const uint16_t release[] = {95, 98, 99};
  TEST_ASSERT_EQUAL(1, feed(k, release, 3));
  TEST_ASSERT_FALSE(k.touched());
}

void test_no_flicker_around_one_limit() {
  // A finger resting lightly makes the reading wander across 65. With one
  // limit, this would flicker on and off. Here it changes once and stays.
  TouchKey k = makeKey();
  const uint16_t wobble[] = {64, 63, 64, 66, 64, 67, 63, 66, 64, 67, 64};
  TEST_ASSERT_EQUAL(1, feed(k, wobble, 11));
  TEST_ASSERT_TRUE(k.touched());
}

void test_single_noisy_reading_ignored() {
  TouchKey k = makeKey();
  const uint16_t spike[] = {100, 30, 100, 100, 20, 21, 100};  // never 3 in a row
  TEST_ASSERT_EQUAL(0, feed(k, spike, 7));
  TEST_ASSERT_FALSE(k.touched());

  const uint16_t touch[] = {40, 40, 40};
  feed(k, touch, 3);
  const uint16_t dropout[] = {40, 100, 40, 40, 100, 100, 40};  // a spike up while touched
  TEST_ASSERT_EQUAL(0, feed(k, dropout, 7));
  TEST_ASSERT_TRUE(k.touched());
}

void test_change_time_is_first_agreeing_reading() {
  TouchKey k = makeKey();
  const uint16_t raw[] = {100, 100, 40, 40, 40};  // touch starts at the 3rd reading
  feed(k, raw, 5, 1000);
  TEST_ASSERT_TRUE(k.touched());
  TEST_ASSERT_EQUAL_UINT32(1006, k.changeMs());  // 1000 + 2 x 3 ms
}

void test_changing_while_readings_build_up() {
  TouchKey k = makeKey();
  TEST_ASSERT_FALSE(k.changing());
  k.update(40, 0);
  TEST_ASSERT_TRUE(k.changing());
  k.update(100, 3);
  TEST_ASSERT_FALSE(k.changing());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_calibration_is_the_average);
  RUN_TEST(test_not_calibrated_never_touches);
  RUN_TEST(test_press_below_65_percent);
  RUN_TEST(test_no_release_until_above_80_percent);
  RUN_TEST(test_no_flicker_around_one_limit);
  RUN_TEST(test_single_noisy_reading_ignored);
  RUN_TEST(test_change_time_is_first_agreeing_reading);
  RUN_TEST(test_changing_while_readings_build_up);
  return UNITY_END();
}
