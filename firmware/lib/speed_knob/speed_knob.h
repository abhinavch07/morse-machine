// Turns raw readings from the speed potentiometer into a steady WPM. It has
// no Arduino code in it, so the unit tests can run on the Mac too.
//
// The ESP32 ADC is noisy: a knob that is not moving can jump by 20 or more
// counts. So two steps keep the WPM still:
//   1. Average the last 16 readings, leaving out the highest and the lowest,
//      so one wild reading is thrown away completely.
//   2. Only move to a new WPM when that average has moved more than
//      `jitter` counts since the last change.

#pragma once

#include <stdint.h>

class SpeedKnob {
 public:
  static const int READINGS = 16;
  static const int ADC_MAX = 4095;  // 12 bit ADC

  // jitter: how far (in ADC counts) the average must move before the WPM
  // can change. Each WPM step is about 4096 / 18 = 228 counts wide.
  SpeedKnob(int minWpm, int maxWpm, int jitter);

  // Add one reading (0 to 4095). Returns true when the WPM just changed.
  // The very first reading fills the whole average, so the WPM is right at
  // once, and that first call returns true.
  bool update(uint16_t raw);

  int wpm() const { return wpm_; }
  int average() const;

 private:
  int toWpm(int average) const;

  int minWpm_;
  int maxWpm_;
  int jitter_;

  uint16_t readings_[READINGS] = {};
  int next_ = 0;             // where the next reading goes
  bool started_ = false;

  int usedAverage_ = 0;      // the average that set the current WPM
  int wpm_ = 0;
};
