// Unit tests for lib/screen_text. They run on the Mac with: pio test -e native
// A line holds 21 letters and the screen shows 4 lines.

#include <unity.h>

#include "screen_text.h"

void setUp() {}
void tearDown() {}

// Adds each char of text as one letter, and a space as a word gap.
void type(ScreenText& s, const char* text) {
  for (int i = 0; text[i] != '\0'; i++) {
    if (text[i] == ' ') {
      s.addSpace();
    } else {
      char letter[2] = {text[i], '\0'};
      s.addLetter(letter);
    }
  }
}

void test_starts_empty() {
  ScreenText s;
  for (int i = 0; i < ScreenText::ROWS; i++) TEST_ASSERT_EQUAL_STRING("", s.line(i));
}

void test_letters_and_spaces_on_first_line() {
  ScreenText s;
  type(s, "CQ DE VU");
  TEST_ASSERT_EQUAL_STRING("CQ DE VU", s.line(0));
  TEST_ASSERT_EQUAL_STRING("", s.line(1));
}

void test_no_space_at_line_start_or_twice() {
  ScreenText s;
  s.addSpace();
  type(s, "K");
  s.addSpace();
  s.addSpace();
  type(s, "R");
  TEST_ASSERT_EQUAL_STRING("K R", s.line(0));
}

void test_word_moves_down_whole() {
  ScreenText s;
  type(s, "AAAAAAAAAA BBBBBBBBBB");  // exactly 21 chars, fills the line
  type(s, "C");                      // too long now, BBBBBBBBBBC moves down
  TEST_ASSERT_EQUAL_STRING("AAAAAAAAAA", s.line(0));
  TEST_ASSERT_EQUAL_STRING("BBBBBBBBBBC", s.line(1));
}

void test_space_at_full_line_starts_new_line() {
  ScreenText s;
  type(s, "AAAAAAAAAAAAAAAAAAAAA K");  // 21 A, then a word gap, then K
  TEST_ASSERT_EQUAL_STRING("AAAAAAAAAAAAAAAAAAAAA", s.line(0));
  TEST_ASSERT_EQUAL_STRING("K", s.line(1));
}

void test_long_word_breaks() {
  ScreenText s;
  type(s, "AAAAAAAAAAAAAAAAAAAAAB");  // 22 letters with no space
  TEST_ASSERT_EQUAL_STRING("AAAAAAAAAAAAAAAAAAAAA", s.line(0));
  TEST_ASSERT_EQUAL_STRING("B", s.line(1));
}

void test_prosign_is_not_split() {
  ScreenText s;
  type(s, "AAAAAAAAAAAAAAAAAAA ");  // 19 letters and a space, 20 chars
  s.addLetter("<AR>");
  TEST_ASSERT_EQUAL_STRING("AAAAAAAAAAAAAAAAAAA", s.line(0));
  TEST_ASSERT_EQUAL_STRING("<AR>", s.line(1));
}

void test_scrolls_to_keep_newest_lines() {
  ScreenText s;
  const char* words[] = {"ONE", "TWO", "THREE", "FOUR", "FIVE"};
  for (int i = 0; i < 5; i++) {
    type(s, words[i]);
    s.newLine();
  }
  type(s, "SIX");
  TEST_ASSERT_EQUAL_STRING("THREE", s.line(0));
  TEST_ASSERT_EQUAL_STRING("FOUR", s.line(1));
  TEST_ASSERT_EQUAL_STRING("FIVE", s.line(2));
  TEST_ASSERT_EQUAL_STRING("SIX", s.line(3));
}

void test_new_line_skips_empty_lines() {
  ScreenText s;
  s.newLine();
  type(s, "K");
  s.newLine();
  s.newLine();
  type(s, "R");
  TEST_ASSERT_EQUAL_STRING("K", s.line(0));
  TEST_ASSERT_EQUAL_STRING("R", s.line(1));
  TEST_ASSERT_EQUAL_STRING("", s.line(2));
}

void test_spaced_code() {
  char out[20];
  TEST_ASSERT_EQUAL_STRING(". - .", spacedCode(".-.", out));
  TEST_ASSERT_EQUAL_STRING("-", spacedCode("-", out));
  TEST_ASSERT_EQUAL_STRING("", spacedCode("", out));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_starts_empty);
  RUN_TEST(test_letters_and_spaces_on_first_line);
  RUN_TEST(test_no_space_at_line_start_or_twice);
  RUN_TEST(test_word_moves_down_whole);
  RUN_TEST(test_space_at_full_line_starts_new_line);
  RUN_TEST(test_long_word_breaks);
  RUN_TEST(test_prosign_is_not_split);
  RUN_TEST(test_scrolls_to_keep_newest_lines);
  RUN_TEST(test_new_line_skips_empty_lines);
  RUN_TEST(test_spaced_code);
  return UNITY_END();
}
