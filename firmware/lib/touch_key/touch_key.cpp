#include "touch_key.h"

TouchKey::TouchKey(float pressRatio, float releaseRatio, int readingsToAgree)
    : pressRatio_(pressRatio),
      releaseRatio_(releaseRatio),
      readingsToAgree_(readingsToAgree) {}

void TouchKey::addCalibrationReading(uint16_t raw) {
  calibrationSum_ += raw;
  calibrationCount_++;
}

void TouchKey::finishCalibration() {
  noTouchLevel_ = (calibrationCount_ > 0)
                      ? (float)calibrationSum_ / calibrationCount_
                      : 0;
}

bool TouchKey::update(uint16_t raw, unsigned long nowMs) {
  // Each state only looks at its own limit. Touched waits to rise above the
  // release limit, not touched waits to drop below the press limit. A
  // reading between the two limits never asks for a change.
  bool wantsChange = touched_ ? (raw > releaseLevel()) : (raw < pressLevel());

  if (!wantsChange) {
    agreeCount_ = 0;  // the run is broken, start counting again next time
    return false;
  }

  if (agreeCount_ == 0) firstAgreeMs_ = nowMs;
  agreeCount_++;
  if (agreeCount_ < readingsToAgree_) return false;

  touched_ = !touched_;
  changeMs_ = firstAgreeMs_;
  agreeCount_ = 0;
  return true;
}
