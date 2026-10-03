// The decoded text as it should look on the OLED: a few short lines that wrap
// and scroll. It has no Arduino or screen code in it, so the unit tests can
// run on the Mac too. main.cpp draws whatever line() returns.

#pragma once

class ScreenText {
 public:
  static const int COLS = 21;  // 128 px wide / 6 px per letter
  static const int ROWS = 4;   // lines of decoded text on the screen

  ScreenText();

  // Adds one decoded letter: "K", a prosign like "<AR>", or "*". If the line
  // is full, the word being written moves down to a new line, so words are
  // not cut in half. A prosign is never split.
  void addLetter(const char* letter);

  // Adds one space for a word gap. Never at the start of a line, and never
  // two in a row.
  void addSpace();

  // Starts a new line, like the serial print after a long pause. Does
  // nothing if the current line is still empty.
  void newLine();

  // The text of one line, 0 is the top. When the screen is full the oldest
  // line scrolls off the top.
  const char* line(int row) const;

 private:
  void startLine();

  char lines_[ROWS][COLS + 1];  // plus the end marker
  int current_;                 // the row being written now
};

// Writes a code like ".-." as ". - .", with a space between symbols so it is
// easy to read on the small screen. out must hold at least
// 2 x strlen(code) chars. Returns out.
char* spacedCode(const char* code, char* out);
