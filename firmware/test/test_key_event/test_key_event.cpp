// Unit tests for lib/key_event. They run on the Mac with: pio test -e native

#include <string.h>
#include <unity.h>

#include "key_event.h"

void setUp() {}
void tearDown() {}

bool parse(const char* text, KeyEvent* event) {
  return parseKeyEvent(text, strlen(text), event);
}

void test_format_matches_the_web_page() {
  KeyEvent e = {true, 12345, "esp32"};
  char out[64];
  TEST_ASSERT_EQUAL(40, formatKeyEvent(e, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("{\"type\":\"down\",\"t\":12345,\"from\":\"esp32\"}", out);

  e.down = false;
  formatKeyEvent(e, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("{\"type\":\"up\",\"t\":12345,\"from\":\"esp32\"}", out);
}

void test_format_needs_room() {
  KeyEvent e = {true, 12345, "esp32"};
  char out[40];  // one too small for the 40 chars plus the end marker
  TEST_ASSERT_EQUAL(0, formatKeyEvent(e, out, sizeof(out)));
}

void test_parse_web_page_event() {
  KeyEvent e;
  TEST_ASSERT_TRUE(parse("{\"type\": \"up\", \"t\": 1234, \"from\": \"phone\"}", &e));
  TEST_ASSERT_FALSE(e.down);
  TEST_ASSERT_EQUAL_UINT32(1234, e.t);
  TEST_ASSERT_EQUAL_STRING("phone", e.from);
}

void test_parse_round_trip() {
  KeyEvent sent = {true, 4000000000UL, "esp32"};  // near the top of millis()
  char text[64];
  formatKeyEvent(sent, text, sizeof(text));
  KeyEvent got;
  TEST_ASSERT_TRUE(parse(text, &got));
  TEST_ASSERT_TRUE(got.down);
  TEST_ASSERT_EQUAL_UINT32(4000000000UL, got.t);
  TEST_ASSERT_EQUAL_STRING("esp32", got.from);
}

void test_missing_from_is_question_mark() {
  KeyEvent e;
  TEST_ASSERT_TRUE(parse("{\"type\":\"down\",\"t\":5}", &e));
  TEST_ASSERT_EQUAL_STRING("?", e.from);
}

void test_long_name_is_cut() {
  KeyEvent e;
  TEST_ASSERT_TRUE(parse("{\"type\":\"down\",\"t\":5,\"from\":\"a_very_long_sender_name\"}", &e));
  TEST_ASSERT_EQUAL_STRING("a_very_long_send", e.from);  // 16 chars
}

void test_rejects_bad_messages() {
  KeyEvent e = {true, 77, "keep"};
  const char* bad[] = {
      "hello",                                // not JSON
      "[1, 2]",                               // not an object
      "{\"t\": 5}",                           // no type
      "{\"type\": \"left\", \"t\": 5}",       // unknown type
      "{\"type\": \"down\"}",                 // no t
      "{\"type\": \"down\", \"t\": \"5\"}",   // t is text
      "{\"type\": \"down\", \"t\": -5}",      // t is negative
      "{\"type\": \"down\", \"t\": 5.5}",     // t is not whole
  };
  for (const char* text : bad) {
    TEST_ASSERT_FALSE_MESSAGE(parse(text, &e), text);
  }
  // A rejected message leaves the event untouched.
  TEST_ASSERT_EQUAL_UINT32(77, e.t);
  TEST_ASSERT_EQUAL_STRING("keep", e.from);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_format_matches_the_web_page);
  RUN_TEST(test_format_needs_room);
  RUN_TEST(test_parse_web_page_event);
  RUN_TEST(test_parse_round_trip);
  RUN_TEST(test_missing_from_is_question_mark);
  RUN_TEST(test_long_name_is_cut);
  RUN_TEST(test_rejects_bad_messages);
  return UNITY_END();
}
