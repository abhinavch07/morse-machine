// Turns "tone on" and "tone off" times from the microphone into letters,
// following the sender's speed. It has no Arduino code in it, so the unit
// tests can run on the Mac too.
//
// It joins two parts: SpeedTracker learns the dit length from each tone, and
// the same KeyDecoder as the key uses turns tones and gaps into letters. The
// decoder limits follow the dit length:
//   dit/dah split at 2 dits, letter gap at 2 dits, word gap at 5 dits.
// These are the halfway points of standard timing (dit 1, dah 3, gaps 1, 3
// and 7).

#pragma once

#include <key_decoder.h>

#include "speed_tracker.h"

class MicDecoder {
 public:
  explicit MicDecoder(float startWpm);

  // The tone turned on or off at atMs. Turning on ends a gap, which can
  // finish a letter or a word, so it returns what was finished.
  DecodeResult toneChanged(bool on, unsigned long atMs);

  // Call often while the tone is off, with the time now, so a letter is
  // finished as soon as the gap is long enough.
  DecodeResult update(unsigned long nowMs);

  float wpm() const { return speed_.wpm(); }
  const char* currentCode() const { return decoder_.currentCode(); }
  bool toneOn() const { return toneOn_; }

  // Time since the last change, for the "new line after 5 seconds" rule.
  unsigned long quietMs(unsigned long nowMs) const;

  void reset();

 private:
  void applyLimits();

  SpeedTracker speed_;
  KeyDecoder decoder_;
  bool toneOn_ = false;
  bool started_ = false;            // seen at least one tone
  unsigned long lastChangeMs_ = 0;
};
