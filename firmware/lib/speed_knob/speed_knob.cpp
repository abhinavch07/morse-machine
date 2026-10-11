#include "speed_knob.h"

#include <stdlib.h>

SpeedKnob::SpeedKnob(int minWpm, int maxWpm, int jitter)
    : minWpm_(minWpm), maxWpm_(maxWpm), jitter_(jitter), wpm_(minWpm) {}

// Equal steps: the full turn is cut into one slice per WPM, 8 to 25 is 18
// slices.
int SpeedKnob::toWpm(int average) const {
  int steps = maxWpm_ - minWpm_ + 1;
  int wpm = minWpm_ + (average * steps) / (ADC_MAX + 1);
  return (wpm > maxWpm_) ? maxWpm_ : wpm;
}

int SpeedKnob::average() const {
  int32_t sum = 0;
  uint16_t lowest = readings_[0];
  uint16_t highest = readings_[0];
  for (int i = 0; i < READINGS; i++) {
    sum += readings_[i];
    if (readings_[i] < lowest) lowest = readings_[i];
    if (readings_[i] > highest) highest = readings_[i];
  }
  return (sum - lowest - highest) / (READINGS - 2);
}

bool SpeedKnob::update(uint16_t raw) {
  if (raw > ADC_MAX) raw = ADC_MAX;

  if (!started_) {
    for (int i = 0; i < READINGS; i++) readings_[i] = raw;
    started_ = true;
    usedAverage_ = average();
    wpm_ = toWpm(usedAverage_);
    return true;
  }

  // The new reading takes the place of the oldest one.
  readings_[next_] = raw;
  next_ = (next_ + 1) % READINGS;

  int avg = average();
  if (abs(avg - usedAverage_) <= jitter_) return false;

  usedAverage_ = avg;
  int wpm = toWpm(avg);
  if (wpm == wpm_) return false;
  wpm_ = wpm;
  return true;
}
