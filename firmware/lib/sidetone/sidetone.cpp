#include "sidetone.h"

#include <math.h>

float envelopeRise(float t, float rampTime) {
  if (t <= 0) return 0;
  if (t >= rampTime) return 1;
  return 0.5f * (1 - cosf((float)M_PI * t / rampTime));
}

float envelopeFall(float t, float rampTime) {
  return envelopeRise(rampTime - t, rampTime);
}

void SidetoneGenerator::begin(float toneHz, float sampleRateHz, float rampMs) {
  for (int i = 0; i < SINE_TABLE_SIZE; i++) {
    sine_[i] = (int8_t)lroundf(127 * sinf(2 * (float)M_PI * i / SINE_TABLE_SIZE));
  }

  rampSamples_ = (int)lroundf(rampMs * sampleRateHz / 1000);
  if (rampSamples_ < 1) rampSamples_ = 1;
  if (rampSamples_ > MAX_RAMP_SAMPLES) rampSamples_ = MAX_RAMP_SAMPLES;
  for (int i = 0; i <= rampSamples_; i++) {
    envelope_[i] = (uint8_t)lroundf(255 * envelopeRise((float)i, (float)rampSamples_));
  }

  // The phase is a 32 bit number that wraps around once per sine wave. Its
  // top 8 bits pick one of the 256 table entries.
  phaseStep_ = (uint32_t)llround((double)toneHz / sampleRateHz * 4294967296.0);
  envPos_ = 0;
  phase_ = 0;
}

uint8_t IRAM_ATTR SidetoneGenerator::nextSample(bool keyDown) {
  // Walk up the ramp while the key is down, back down when it is up. If the
  // key is let go half way up, the fall starts from where the rise was.
  if (keyDown) {
    if (envPos_ < rampSamples_) envPos_++;
  } else if (envPos_ > 0) {
    envPos_--;
  }

  if (envPos_ == 0) {
    phase_ = 0;  // so the next tone starts at the zero point of the wave
    return DAC_MID;
  }

  int sample = sine_[phase_ >> 24];
  phase_ += phaseStep_;
  return (uint8_t)(DAC_MID + sample * envelope_[envPos_] / 255);
}
