#include "goertzel.h"

#include <math.h>

static const float PI_F = 3.14159265f;

Goertzel::Goertzel(float targetHz, float sampleRateHz, int blockSize)
    : targetHz_(targetHz),
      blockSize_(blockSize),
      coeff_(2.0f * cosf(2.0f * PI_F * targetHz / sampleRateHz)) {}

bool Goertzel::addSample(float x) {
  // The filter "rings" at targetHz: samples at that frequency keep adding up
  // in step, other frequencies mostly cancel out.
  float s0 = x + coeff_ * s1_ - s2_;
  s2_ = s1_;
  s1_ = s0;
  if (++count_ < blockSize_) return false;

  // Power at targetHz from the last two values. Scaling by 2 / blockSize
  // turns it into the plain amplitude of the sine.
  float power = s1_ * s1_ + s2_ * s2_ - coeff_ * s1_ * s2_;
  if (power < 0) power = 0;  // rounding can make a tiny negative number
  strength_ = 2.0f * sqrtf(power) / blockSize_;

  s1_ = 0;
  s2_ = 0;
  count_ = 0;
  return true;
}
