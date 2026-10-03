#include "screen_text.h"

#include <string.h>

ScreenText::ScreenText() : current_(0) {
  for (int i = 0; i < ROWS; i++) lines_[i][0] = '\0';
}

void ScreenText::addLetter(const char* letter) {
  int length = strlen(letter);
  if (length > COLS) return;  // cannot happen with real letters, but stay safe

  char* row = lines_[current_];
  if ((int)strlen(row) + length > COLS) {
    // Take the unfinished word (after the last space) off this line, so it
    // can go to the next line whole. The space itself is dropped.
    char word[COLS + 1] = "";
    char* space = strrchr(row, ' ');
    if (space != nullptr) {
      strcpy(word, space + 1);
      *space = '\0';
    }
    startLine();
    strcpy(lines_[current_], word);

    // One word filling a whole line. Nothing to do but break it here.
    if ((int)strlen(lines_[current_]) + length > COLS) startLine();
  }
  strcat(lines_[current_], letter);
}

void ScreenText::addSpace() {
  char* row = lines_[current_];
  int length = strlen(row);
  if (length == 0 || row[length - 1] == ' ') return;

  // A full line already ends the word, so the next line starts clean.
  if (length == COLS) {
    startLine();
    return;
  }
  strcat(row, " ");
}

void ScreenText::newLine() {
  if (lines_[current_][0] != '\0') startLine();
}

const char* ScreenText::line(int row) const {
  if (row < 0 || row >= ROWS) return "";
  return lines_[row];
}

void ScreenText::startLine() {
  if (current_ < ROWS - 1) {
    current_++;
  } else {
    // Full: move every line up one and forget the top one.
    for (int i = 0; i < ROWS - 1; i++) strcpy(lines_[i], lines_[i + 1]);
  }
  lines_[current_][0] = '\0';
}

char* spacedCode(const char* code, char* out) {
  int n = 0;
  for (int i = 0; code[i] != '\0'; i++) {
    if (i > 0) out[n++] = ' ';
    out[n++] = code[i];
  }
  out[n] = '\0';
  return out;
}
