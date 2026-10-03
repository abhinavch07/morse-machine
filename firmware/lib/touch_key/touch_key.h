// Turns raw touch readings into a clean "touched" or "not touched". It has no
// Arduino code in it, so the unit tests can run on the Mac too. main.cpp
// reads the pad and feeds the numbers in.
//
// A touch makes the reading drop. Two limits stop flicker (hysteresis):
//   touched when the reading goes below pressRatio x the no touch level,
//   released when it goes back above releaseRatio x the no touch level.
// In between, nothing changes. On top of that, a change only happens after
// several readings in a row agree, so one noisy reading is ignored.

#pragma once

#include <stdint.h>

class TouchKey {
 public:
  // pressRatio:      for example 0.65, touched below 65% of the no touch level.
  // releaseRatio:    for example 0.80, released above 80%. Must be higher.
  // readingsToAgree: how many readings in a row must agree, for example 3.
  TouchKey(float pressRatio, float releaseRatio, int readingsToAgree);

  // Calibration: call addCalibrationReading() many times with nobody
  // touching, then finishCalibration() once to set the no touch level.
  void addCalibrationReading(uint16_t raw);
  void finishCalibration();

  float noTouchLevel() const { return noTouchLevel_; }
  float pressLevel() const { return noTouchLevel_ * pressRatio_; }
  float releaseLevel() const { return noTouchLevel_ * releaseRatio_; }

  // Feed one reading taken at nowMs. Returns true if the state just changed.
  bool update(uint16_t raw, unsigned long nowMs);

  bool touched() const { return touched_; }

  // When the change that just happened began: the time of the first of the
  // readings that agreed. Using this instead of the time of the last one
  // means the smoothing delay does not change measured press or gap times.
  unsigned long changeMs() const { return changeMs_; }

  // True while some readings already point to a change, but not enough yet.
  bool changing() const { return agreeCount_ > 0; }

 private:
  float pressRatio_;
  float releaseRatio_;
  int readingsToAgree_;

  uint32_t calibrationSum_ = 0;
  uint32_t calibrationCount_ = 0;
  float noTouchLevel_ = 0;  // 0 until calibrated, so it never reads as touched

  bool touched_ = false;
  int agreeCount_ = 0;           // readings in a row that point to a change
  unsigned long firstAgreeMs_ = 0;
  unsigned long changeMs_ = 0;
};
