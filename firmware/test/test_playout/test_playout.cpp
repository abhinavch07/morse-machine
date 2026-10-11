// Unit tests for lib/playout. They run on the Mac with: pio test -e native
// The planPlay() tests copy the Playout tests in tests/morse_js.test.mjs, so
// the ESP32 and the web page follow the same rules.
// Decoder limits match main.cpp: split 160 ms, letter gap 250, word gap 2500.

#include <string.h>
#include <unity.h>

#include "playout.h"

void setUp() {}
void tearDown() {}

// ---- planPlay(): when each event plays ----

void test_first_event_plays_after_the_delay() {
  uint32_t offset = 0;
  PlayTime p = planPlay(false, &offset, 50000, 1000);
  TEST_ASSERT_EQUAL_UINT32(1200, p.playAt);
  TEST_ASSERT_TRUE(p.reset);
}

void test_later_events_keep_the_senders_gaps() {
  uint32_t offset = 0;
  planPlay(false, &offset, 50000, 1000);  // plays at 1200
  // Sent 240 ms later, arrived 300 ms later, still plays 240 ms after the first.
  PlayTime p = planPlay(true, &offset, 50240, 1300);
  TEST_ASSERT_EQUAL_UINT32(1440, p.playAt);
  TEST_ASSERT_FALSE(p.reset);
}

void test_late_event_resets_the_delay() {
  uint32_t offset = 0;
  planPlay(false, &offset, 50000, 1000);
  PlayTime p = planPlay(true, &offset, 50080, 1500);  // slot was 1280
  TEST_ASSERT_EQUAL_UINT32(1700, p.playAt);
  TEST_ASSERT_TRUE(p.reset);
  p = planPlay(true, &offset, 50160, 1580);  // uses the new offset
  TEST_ASSERT_EQUAL_UINT32(1780, p.playAt);
  TEST_ASSERT_FALSE(p.reset);
}

void test_jump_far_ahead_resets_the_delay() {
  uint32_t offset = 0;
  planPlay(false, &offset, 1000, 1000);
  PlayTime p = planPlay(true, &offset, 900000, 2000);  // the sender reloaded its page
  TEST_ASSERT_EQUAL_UINT32(2200, p.playAt);
  TEST_ASSERT_TRUE(p.reset);
}

void test_works_across_the_millis_wrap() {
  // Our millis() is about to wrap past 4294967295 back to 0.
  uint32_t offset = 0;
  uint32_t now = 4294967000UL;
  planPlay(false, &offset, 1000, now);  // plays at now + 200
  PlayTime p = planPlay(true, &offset, 1500, now + 400);
  TEST_ASSERT_EQUAL_UINT32(now + 700, p.playAt);  // 404 after the wrap
  TEST_ASSERT_FALSE(p.reset);
}

// ---- RemoteKeys: playing and decoding ----

KeyEvent ev(const char* from, bool down, uint32_t t) {
  KeyEvent e = {down, t, ""};
  strncpy(e.from, from, MAX_SENDER_NAME);
  return e;
}

// Collected output, written like the OLED shows it: the sender's first
// letter and a colon whenever the sender changes.
char text[200];
char lastSender[MAX_SENDER_NAME + 1];

void collect(RemoteKeys& r, uint32_t nowMs) {
  Received got;
  while (r.update(nowMs, &got)) {
    if (got.result.letterDone) {
      if (strcmp(got.sender, lastSender) != 0) {
        size_t n = strlen(text);
        text[n] = got.sender[0];
        text[n + 1] = ':';
        text[n + 2] = '\0';
        strcpy(lastSender, got.sender);
      }
      strcat(text, got.result.letter);
    }
    if (got.result.wordDone) strcat(text, " ");
  }
}

// Runs the clock from fromMs to toMs, 1 ms at a time, like loop() does.
void runUntil(RemoteKeys& r, uint32_t fromMs, uint32_t toMs) {
  for (uint32_t now = fromMs; now <= toMs; now++) collect(r, now);
}

RemoteKeys makeKeys() {
  text[0] = '\0';
  lastSender[0] = '\0';
  return RemoteKeys(160, 250, 2500);
}

// K (-.-) at 15 WPM as the sender's t values: dah 240, gap 80, dit 80, gap 80, dah 240.
const uint32_t K_TIMES[] = {0, 240, 320, 400, 480, 720};

void test_decodes_k_with_even_network() {
  RemoteKeys r = makeKeys();
  for (int i = 0; i < 6; i++) {
    // Each event arrives 30 ms after it was sent.
    r.receive(ev("phone", i % 2 == 0, 10000 + K_TIMES[i]), 1000 + K_TIMES[i] + 30);
    runUntil(r, 1000 + K_TIMES[i] + 30, 1000 + K_TIMES[i] + 30);
  }
  runUntil(r, 1751, 5000);
  TEST_ASSERT_EQUAL_STRING("p:K ", text);
}

void test_uneven_network_does_not_change_the_text() {
  // Arrival delays jump between 5 and 150 ms, all inside the 200 ms buffer.
  const uint32_t lag[] = {5, 150, 20, 140, 10, 120};
  RemoteKeys r = makeKeys();
  uint32_t now = 1000;
  for (int i = 0; i < 6; i++) {
    uint32_t arrives = 1000 + K_TIMES[i] + lag[i];
    if ((int32_t)(arrives - now) < 0) arrives = now;  // messages never overtake
    runUntil(r, now, arrives);
    r.receive(ev("phone", i % 2 == 0, 10000 + K_TIMES[i]), arrives);
    now = arrives;
  }
  runUntil(r, now, now + 4000);
  TEST_ASSERT_EQUAL_STRING("p:K ", text);
}

void test_tone_follows_playout_delay() {
  RemoteKeys r = makeKeys();
  r.receive(ev("phone", true, 10000), 1000);  // plays at 1200
  r.receive(ev("phone", false, 10240), 1240);  // plays at 1440
  runUntil(r, 1000, 1199);
  TEST_ASSERT_FALSE(r.toneOn(1199));  // not yet: still in the buffer
  runUntil(r, 1200, 1200);
  TEST_ASSERT_TRUE(r.toneOn(1200));
  runUntil(r, 1201, 1440);
  TEST_ASSERT_FALSE(r.toneOn(1440));
}

void test_stuck_key_stops_the_tone_but_still_decodes() {
  RemoteKeys r = makeKeys();
  r.receive(ev("phone", true, 10000), 1000);  // plays at 1200
  runUntil(r, 1000, 3199);
  TEST_ASSERT_TRUE(r.toneOn(3199));
  TEST_ASSERT_FALSE(r.toneOn(3200));  // 2000 ms after it started playing
  // The up finally comes: a 2500 ms press is still a dah, so T.
  r.receive(ev("phone", false, 12500), 3500);
  runUntil(r, 3500, 6000);
  TEST_ASSERT_EQUAL_STRING("p:T", text);
}

void test_two_senders_decode_separately() {
  RemoteKeys r = makeKeys();
  // phone sends E (a dit), laptop sends T (a dah) at the same time.
  r.receive(ev("phone", true, 500), 1000);
  r.receive(ev("laptop", true, 90000), 1000);
  r.receive(ev("phone", false, 580), 1080);
  r.receive(ev("laptop", false, 90240), 1240);
  runUntil(r, 1000, 2000);
  TEST_ASSERT_EQUAL_STRING("p:El:T", text);
}

void test_tone_on_while_any_sender_is_down() {
  RemoteKeys r = makeKeys();
  r.receive(ev("phone", true, 0), 1000);       // down 1200 to 1300
  r.receive(ev("phone", false, 100), 1100);
  r.receive(ev("laptop", true, 0), 1050);      // down 1250 to 1500
  r.receive(ev("laptop", false, 250), 1300);
  runUntil(r, 1000, 1350);
  TEST_ASSERT_TRUE(r.toneOn(1350));  // phone is up, laptop still down
  runUntil(r, 1351, 1500);
  TEST_ASSERT_FALSE(r.toneOn(1500));
}

void test_repeats_and_up_without_down_are_ignored() {
  RemoteKeys r = makeKeys();
  r.receive(ev("phone", false, 0), 1000);      // we joined mid press
  r.receive(ev("phone", true, 400), 1400);
  r.receive(ev("phone", true, 420), 1420);     // a repeat
  r.receive(ev("phone", false, 640), 1640);    // pressed 240 ms from 400: a dah
  runUntil(r, 1000, 4000);
  TEST_ASSERT_EQUAL_STRING("p:T", text);
}

void test_fifth_sender_replaces_the_quietest() {
  RemoteKeys r = makeKeys();
  const char* names[] = {"a", "b", "c", "d"};
  for (int i = 0; i < 4; i++) r.receive(ev(names[i], true, 0), 1000 + i);
  // "a" was heard first and longest ago, so "e" takes its place. a's planned
  // event must not play as e's.
  r.receive(ev("e", true, 0), 1010);
  r.receive(ev("e", false, 80), 1090);
  runUntil(r, 1000, 2000);
  TEST_ASSERT_EQUAL_STRING("e:E", text);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_first_event_plays_after_the_delay);
  RUN_TEST(test_later_events_keep_the_senders_gaps);
  RUN_TEST(test_late_event_resets_the_delay);
  RUN_TEST(test_jump_far_ahead_resets_the_delay);
  RUN_TEST(test_works_across_the_millis_wrap);
  RUN_TEST(test_decodes_k_with_even_network);
  RUN_TEST(test_uneven_network_does_not_change_the_text);
  RUN_TEST(test_tone_follows_playout_delay);
  RUN_TEST(test_stuck_key_stops_the_tone_but_still_decodes);
  RUN_TEST(test_two_senders_decode_separately);
  RUN_TEST(test_tone_on_while_any_sender_is_down);
  RUN_TEST(test_repeats_and_up_without_down_are_ignored);
  RUN_TEST(test_fifth_sender_replaces_the_quietest);
  return UNITY_END();
}
