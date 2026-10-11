// Unit tests for lib/iambic_keyer. They run on the Mac with: pio test -e native
// All at 15 WPM: 1 unit = 1200 / 15 = 80 ms. Dit 80, dah 240, gap 80.
// The paddles are given as "held from ms to ms", and the keyer is fed once
// every ms, like a busy loop() would.

#include <unity.h>

#include <string.h>

#include "iambic_keyer.h"

void setUp() {}
void tearDown() {}

// One paddle held from startMs up to (not including) endMs.
struct Hold {
  unsigned long startMs;
  unsigned long endMs;
};

const Hold NEVER = {0, 0};

bool held(const Hold& h, unsigned long t) { return t >= h.startMs && t < h.endMs; }

// What the keyer did: a string of '.' and '-' for the elements, and the
// time of every key line change.
struct Run {
  char code[32];
  unsigned long changes[64];
  int changeCount;
};

Run run(IambicKeyer& k, Hold dit, Hold dah, unsigned long untilMs) {
  Run r = {};
  unsigned long downAt = 0;
  for (unsigned long t = 0; t <= untilMs; t++) {
    while (k.update(held(dit, t), held(dah, t), t)) {
      r.changes[r.changeCount++] = k.changeMs();
      if (k.keyDown()) {
        downAt = k.changeMs();
      } else {
        // A press of 1 unit is a dit, 3 units a dah.
        size_t n = strlen(r.code);
        r.code[n] = (k.changeMs() - downAt < 160) ? '.' : '-';
      }
    }
  }
  return r;
}

void test_unit_is_1200_over_wpm() {
  IambicKeyer k(15);
  TEST_ASSERT_EQUAL_FLOAT(80, k.unitMs());
  k.setWpm(25);
  TEST_ASSERT_EQUAL_FLOAT(48, k.unitMs());
}

void test_single_dit_is_80_ms() {
  IambicKeyer k(15);
  Run r = run(k, {10, 30}, NEVER, 1000);
  TEST_ASSERT_EQUAL_STRING(".", r.code);
  TEST_ASSERT_EQUAL(2, r.changeCount);
  TEST_ASSERT_EQUAL(10, r.changes[0]);
  TEST_ASSERT_EQUAL(90, r.changes[1]);
  TEST_ASSERT_FALSE(k.busy());
}

void test_single_dah_is_240_ms() {
  IambicKeyer k(15);
  Run r = run(k, NEVER, {10, 30}, 1000);
  TEST_ASSERT_EQUAL_STRING("-", r.code);
  TEST_ASSERT_EQUAL(2, r.changeCount);
  TEST_ASSERT_EQUAL(10, r.changes[0]);
  TEST_ASSERT_EQUAL(250, r.changes[1]);
}

// Held for 400 ms: dits start at 0, 160, 320. Released at 400, during the
// third dit's gap, so no fourth dit.
void test_holding_dit_repeats_dits() {
  IambicKeyer k(15);
  Run r = run(k, {0, 400}, NEVER, 1000);
  TEST_ASSERT_EQUAL_STRING("...", r.code);
  const unsigned long expected[] = {0, 80, 160, 240, 320, 400};
  TEST_ASSERT_EQUAL(6, r.changeCount);
  TEST_ASSERT_EQUAL_UINT32_ARRAY(expected, r.changes, 6);
}

// Dahs start at 0 and 320. Released at 500, during the second dah.
void test_holding_dah_repeats_dahs() {
  IambicKeyer k(15);
  Run r = run(k, NEVER, {0, 500}, 1000);
  TEST_ASSERT_EQUAL_STRING("--", r.code);
  const unsigned long expected[] = {0, 240, 320, 560};
  TEST_ASSERT_EQUAL(4, r.changeCount);
  TEST_ASSERT_EQUAL_UINT32_ARRAY(expected, r.changes, 4);
}

// Squeeze, dit first: dit at 0, dah at 160, dit at 480, dah at 640 ...
void test_squeeze_starting_with_dit_alternates() {
  IambicKeyer k(15);
  Run r = run(k, {0, 700}, {20, 700}, 2000);
  TEST_ASSERT_EQUAL_STRING(".-.-", r.code);
  TEST_ASSERT_EQUAL(0, r.changes[0]);
  TEST_ASSERT_EQUAL(160, r.changes[2]);
  TEST_ASSERT_EQUAL(480, r.changes[4]);
  TEST_ASSERT_EQUAL(640, r.changes[6]);
}

void test_squeeze_starting_with_dah_alternates() {
  IambicKeyer k(15);
  Run r = run(k, {20, 900}, {0, 900}, 2000);
  TEST_ASSERT_EQUAL_STRING("-.-.", r.code);
  TEST_ASSERT_EQUAL(0, r.changes[0]);
  TEST_ASSERT_EQUAL(320, r.changes[2]);
  TEST_ASSERT_EQUAL(480, r.changes[4]);
}

// Both pressed in the same ms from rest: dit goes first.
void test_squeeze_from_rest_starts_with_dit() {
  IambicKeyer k(15);
  Run r = run(k, {0, 200}, {0, 200}, 1000);
  TEST_ASSERT_EQUAL_STRING(".-", r.code);
}

// C is -.-. : dah at 0, dit at 320, dah at 480, dit at 800 to 880.
// Mode A: let go of both during the last dit, and it stops there.
void test_letter_c_by_squeeze_mode_a() {
  IambicKeyer k(15);
  Run r = run(k, {100, 840}, {0, 840}, 2000);
  TEST_ASSERT_EQUAL_STRING("-.-.", r.code);
  TEST_ASSERT_EQUAL(880, r.changes[7]);
}

// Mode B: let go one element earlier, during the second dah (480 to 720),
// and Mode B adds the last dit by itself.
void test_letter_c_by_squeeze_mode_b() {
  IambicKeyer k(15);
  k.setMode(KEYER_MODE_B);
  Run r = run(k, {100, 600}, {0, 600}, 2000);
  TEST_ASSERT_EQUAL_STRING("-.-.", r.code);
  TEST_ASSERT_EQUAL(880, r.changes[7]);
}

// Dah memory: hold dit, tap dah for 20 ms during the dit, let go of both
// before the dit ends. The dah is remembered and sent: .- is A.
void test_dah_memory() {
  IambicKeyer k(15);
  Run r = run(k, {0, 60}, {30, 50}, 1000);
  TEST_ASSERT_EQUAL_STRING(".-", r.code);
  TEST_ASSERT_EQUAL(160, r.changes[2]);
}

// Dit memory: hold dah, tap dit for 20 ms during the dah. The dit comes next,
// even though the dah is still held, then dahs go on: -.- is K.
void test_dit_memory() {
  IambicKeyer k(15);
  Run r = run(k, {100, 120}, {0, 500}, 1500);
  TEST_ASSERT_EQUAL_STRING("-.-", r.code);
  TEST_ASSERT_EQUAL(320, r.changes[2]);
  TEST_ASSERT_EQUAL(480, r.changes[4]);
}

// A tap that ends in the gap is not lost either.
void test_memory_during_gap() {
  IambicKeyer k(15);
  Run r = run(k, {0, 30}, {100, 120}, 1000);  // gap is 80 to 160
  TEST_ASSERT_EQUAL_STRING(".-", r.code);
}

// Squeeze from dit, let go of both during the dah (160 to 400).
// Mode A finishes the dah and stops: .- is A.
void test_mode_a_stops_on_release() {
  IambicKeyer k(15);
  Run r = run(k, {0, 300}, {20, 300}, 2000);
  TEST_ASSERT_EQUAL_STRING(".-", r.code);
}

// The same hands in Mode B add one more dit: .-. is R.
void test_mode_b_adds_opposite_on_release() {
  IambicKeyer k(15);
  k.setMode(KEYER_MODE_B);
  Run r = run(k, {0, 300}, {20, 300}, 2000);
  TEST_ASSERT_EQUAL_STRING(".-.", r.code);
  TEST_ASSERT_EQUAL(480, r.changes[4]);
}

// With only one paddle, Mode B behaves just like Mode A.
void test_mode_b_single_paddle_no_extra() {
  IambicKeyer k(15);
  k.setMode(KEYER_MODE_B);
  Run r = run(k, {0, 30}, NEVER, 1000);
  TEST_ASSERT_EQUAL_STRING(".", r.code);
}

// A speed change in the middle of a dah does not change that dah.
void test_speed_change_waits_for_next_element() {
  IambicKeyer k(15);
  unsigned long t = 0;
  TEST_ASSERT_TRUE(k.update(false, true, t));   // dah starts at 0
  k.setWpm(20);                                 // 60 ms units from now on
  while (!k.update(false, true, ++t)) {}
  TEST_ASSERT_EQUAL(240, k.changeMs());         // still a 15 WPM dah
  while (!k.update(false, true, ++t)) {}
  TEST_ASSERT_EQUAL(320, k.changeMs());         // and its 80 ms gap
  while (!k.update(false, true, ++t)) {}
  TEST_ASSERT_EQUAL(500, k.changeMs());         // next dah is 180 ms
}

// If update() is called late, the times are still exact and nothing is lost.
void test_late_update_keeps_exact_times() {
  IambicKeyer k(15);
  TEST_ASSERT_TRUE(k.update(true, false, 0));
  TEST_ASSERT_TRUE(k.update(true, false, 175));  // 15 ms after the next dit was due
  TEST_ASSERT_FALSE(k.keyDown());
  TEST_ASSERT_EQUAL(80, k.changeMs());
  TEST_ASSERT_TRUE(k.update(true, false, 175));
  TEST_ASSERT_TRUE(k.keyDown());
  TEST_ASSERT_EQUAL(160, k.changeMs());
  TEST_ASSERT_FALSE(k.update(true, false, 175));
}

// millis() goes back to 0 after about 49 days. Timing must not break.
void test_millis_wrap() {
  IambicKeyer k(15);
  unsigned long start = 0xFFFFFFFFUL - 30;
  TEST_ASSERT_TRUE(k.update(true, false, start));
  TEST_ASSERT_FALSE(k.update(false, false, start + 40));
  TEST_ASSERT_TRUE(k.update(false, false, start + 80));
  TEST_ASSERT_EQUAL(start + 80, k.changeMs());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_unit_is_1200_over_wpm);
  RUN_TEST(test_single_dit_is_80_ms);
  RUN_TEST(test_single_dah_is_240_ms);
  RUN_TEST(test_holding_dit_repeats_dits);
  RUN_TEST(test_holding_dah_repeats_dahs);
  RUN_TEST(test_squeeze_starting_with_dit_alternates);
  RUN_TEST(test_squeeze_starting_with_dah_alternates);
  RUN_TEST(test_squeeze_from_rest_starts_with_dit);
  RUN_TEST(test_letter_c_by_squeeze_mode_a);
  RUN_TEST(test_letter_c_by_squeeze_mode_b);
  RUN_TEST(test_dah_memory);
  RUN_TEST(test_dit_memory);
  RUN_TEST(test_memory_during_gap);
  RUN_TEST(test_mode_a_stops_on_release);
  RUN_TEST(test_mode_b_adds_opposite_on_release);
  RUN_TEST(test_mode_b_single_paddle_no_extra);
  RUN_TEST(test_speed_change_waits_for_next_element);
  RUN_TEST(test_late_update_keeps_exact_times);
  RUN_TEST(test_millis_wrap);
  return UNITY_END();
}
