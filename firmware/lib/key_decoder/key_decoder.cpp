#include "key_decoder.h"

#include <morse.h>

KeyDecoder::KeyDecoder(float ditDahSplitMs, float letterGapMs, float wordGapMs)
    : ditDahSplitMs(ditDahSplitMs),
      letterGapMs(letterGapMs),
      wordGapMs(wordGapMs),
      symbolCount(0),
      lettersInWord(false) {
  code[0] = '\0';
}

void KeyDecoder::keyDown(float ms) {
  // Too many presses for any real code. Keep counting so the letter still
  // comes out as unknown, but do not write past the end of the buffer.
  if (symbolCount < MAX_SYMBOLS) {
    code[symbolCount] = (ms < ditDahSplitMs) ? '.' : '-';
    code[symbolCount + 1] = '\0';
  }
  symbolCount++;
}

DecodeResult KeyDecoder::keyUp(float ms) {
  DecodeResult result = {false, false, nullptr, nullptr};

  // Once a letter is done symbolCount is 0, so it cannot be reported twice.
  if (symbolCount > 0 && ms >= letterGapMs) {
    const char* text = (symbolCount > MAX_SYMBOLS) ? nullptr : morseDecode(code);
    result.letterDone = true;
    result.letter = (text != nullptr) ? text : "*";
    result.code = code;
    symbolCount = 0;
    lettersInWord = true;
  }

  // Same idea for the word gap: only once, and only after a letter.
  if (lettersInWord && ms >= wordGapMs) {
    result.wordDone = true;
    lettersInWord = false;
  }

  return result;
}
