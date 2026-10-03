// Sidetone maths: a sine wave with a raised cosine envelope, as 8 bit DAC
// values. No Arduino code here, so the unit tests can run on the Mac too.

#pragma once

#include <stdint.h>

// IRAM_ATTR puts a function in the ESP32's fast internal RAM, so it still runs
// when the flash chip is busy. On the Mac it means nothing, so it is empty.
#ifdef ESP_PLATFORM
#include <esp_attr.h>
#else
#define IRAM_ATTR
#endif

const uint8_t DAC_MID = 128;        // the middle DAC level, used for silence
const int SINE_TABLE_SIZE = 256;    // one full sine wave in 256 steps
const int MAX_RAMP_SAMPLES = 400;   // room for a 10 ms ramp at 40 kHz

// Raised cosine rise, the same shape as audio.py: 0 at t = 0, half at
// rampTime / 2, and 1 from rampTime on. t and rampTime use the same unit.
float envelopeRise(float t, float rampTime);

// The rise played backwards: 1 at t = 0, half at rampTime / 2, 0 at rampTime.
float envelopeFall(float t, float rampTime);

// Makes one DAC value per sample. begin() fills the tables using floats.
// nextSample() then uses whole numbers only, because the ESP32 cannot use
// floats safely inside a timer interrupt. It is in IRAM and calls nothing
// else. The tables are plain arrays inside the object, so when the object is
// a global they live in RAM, not in flash.
class SidetoneGenerator {
 public:
  void begin(float toneHz, float sampleRateHz, float rampMs);

  // The next DAC value, 0 to 255. Call it once per sample.
  uint8_t nextSample(bool keyDown);

 private:
  int8_t sine_[SINE_TABLE_SIZE];             // -127 to 127
  uint8_t envelope_[MAX_RAMP_SAMPLES + 1];   // 0 (silent) to 255 (full)
  int rampSamples_ = 1;
  int envPos_ = 0;           // where we are on the ramp, 0 means silent
  uint32_t phase_ = 0;       // where we are in the sine wave
  uint32_t phaseStep_ = 0;   // how far to move each sample, sets the pitch
};
