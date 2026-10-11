// The Goertzel filter: measures how strong one frequency is in a block of
// audio samples. It has no Arduino code in it, so the unit tests can run on
// the Mac too.
//
// It is like one bar of a spectrum display. A full FFT would work out every
// frequency at once, but we only care about a few, and Goertzel needs just
// one multiply and two adds per sample for each frequency.
//
// Feed it one sample at a time. After blockSize samples, it reports the
// strength and starts the next block on its own.

#pragma once

class Goertzel {
 public:
  // At 8000 samples a second, a block of 80 samples is 10 ms. The filter is
  // then about 100 Hz wide (sampleRate / blockSize).
  Goertzel(float targetHz, float sampleRateHz, int blockSize);

  // Add one sample. Returns true when a block just finished, see strength().
  bool addSample(float x);

  // The tone's amplitude in the last block, in the same units as the
  // samples. A clean sine of amplitude 100 exactly at targetHz gives 100.
  float strength() const { return strength_; }

  float targetHz() const { return targetHz_; }

 private:
  float targetHz_;
  int blockSize_;
  float coeff_;      // 2 x cos(2 x pi x targetHz / sampleRate), worked out once

  float s1_ = 0;     // the last two values of the filter
  float s2_ = 0;
  int count_ = 0;    // samples so far in this block
  float strength_ = 0;
};
