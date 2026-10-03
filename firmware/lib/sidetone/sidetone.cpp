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

void SidetoneGenerator::begin(float toneHz, float sampleRateHz, float rampMs,
                              ToneShape shape) {
  float third = 0, fifth = 0;  // TONE_SOFT: no harmonics
  if (shape == TONE_BRIGHT) { third = 0.25f; fifth = 0.10f; }
  if (shape == TONE_SHARP) { third = 0.33f; fifth = 0.20f; }

  // Add the waves together, then find the biggest swing up or down.
  float raw[SINE_TABLE_SIZE];
  float peak = 0;
  for (int i = 0; i < SINE_TABLE_SIZE; i++) {
    float x = 2 * (float)M_PI * i / SINE_TABLE_SIZE;
    raw[i] = sinf(x) + third * sinf(3 * x) + fifth * sinf(5 * x);
    if (fabsf(raw[i]) > peak) peak = fabsf(raw[i]);
  }

  // Scale so the biggest swing is exactly 127. Then 128 plus any entry stays
  // inside 1 to 255, so the DAC never clips.
  for (int i = 0; i < SINE_TABLE_SIZE; i++) {
    wave_[i] = (int8_t)lroundf(127 * raw[i] / peak);
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

  int sample = wave_[phase_ >> 24];
  phase_ += phaseStep_;
  return (uint8_t)(DAC_MID + sample * envelope_[envPos_] / 255);
}
