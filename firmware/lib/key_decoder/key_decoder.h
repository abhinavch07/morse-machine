// Turns key timings into letters. It has no Arduino code in it, so the unit
// tests can run on the Mac too.
//
// Feed it two kinds of event:
//   keyDown(ms): the key was just released after being down for ms.
//   keyUp(ms):   the key has now been up for ms. Call this again and again
//                while the key stays up, with the time so far, so a letter can
//                finish as soon as the gap is long enough.
// The three limits are set separately in ms, so they can fit a real hand,
// which often has normal presses but longer gaps.

#pragma once

// What a call to keyUp() finished, if anything. Both can be true in one call
// if the gap jumped straight past the word gap limit.
struct DecodeResult {
  bool letterDone;     // a letter just finished, see letter and code
  bool wordDone;       // a word gap just finished
  const char* letter;  // "K", "<AR>", or "*" for an unknown code
  const char* code;    // the dits and dahs heard, like "-.-". Print it
                       // straight away: the next keyDown() overwrites it.
};

class KeyDecoder {
 public:
  // ditDahSplitMs: a press this long or longer is a dah, shorter is a dit.
  // letterGapMs:   a gap this long or longer ends the letter.
  // wordGapMs:     a gap this long or longer ends the word.
  KeyDecoder(float ditDahSplitMs, float letterGapMs, float wordGapMs);

  // Adds a dit or a dah to the letter.
  void keyDown(float ms);

  // Reports a finished letter or word once the gap is long enough. Each one
  // is reported only once per gap.
  DecodeResult keyUp(float ms);

 private:
  static const int MAX_SYMBOLS = 9;  // longer than any code in the table

  float ditDahSplitMs;
  float letterGapMs;
  float wordGapMs;
  char code[MAX_SYMBOLS + 1];   // dits and dahs so far, plus the end marker
  int symbolCount;              // how many presses in this letter
  bool lettersInWord;           // a letter came since the last word gap
};
