// Turns raw microphone samples into "tone on" or "tone off", once every
// 10 ms block. It has no Arduino code in it, so the unit tests can run on the
// Mac too.
//
// Steps for every block:
//   1. Take away the steady DC level. The MAX9814 output sits at 1.25 V,
//      about 1550 ADC counts, and only the swing around it is sound.
//   2. Run 11 Goertzel filters, 400 to 900 Hz in 50 Hz steps.
//   3. Pick the filter to listen to. Fixed mode always uses the fixed
//      frequency (600 Hz). Auto mode moves to whichever filter has been
//      clearly strongest for a while.
//   4. Compare its strength with two levels worked out from the background
//      noise (from the weakest filters, see the .cpp) and the recent tone
//      peak. It turns on above the higher level
//      and off below the lower one (hysteresis), so it does not flicker.
//   5. A change only counts after 2 blocks in a row agree, so one noisy
//      block is ignored. The change time is the first of those blocks, so
//      tone lengths are not stretched.

#pragma once

#include <goertzel.h>

class ToneDetector {
 public:
  static const int BIN_COUNT = 11;
  static const int LOWEST_HZ = 400;
  static const int STEP_HZ = 50;

  ToneDetector(float sampleRateHz, int blockSize, float fixedHz);

  void setAutoMode(bool on);
  bool autoMode() const { return autoMode_; }

  // While ignored, the tone is held off and the noise and peak levels are
  // left alone. Used while my own sidetone plays, so the mic does not hear it.
  void setIgnored(bool ignored) { ignored_ = ignored; }

  // Start again from scratch, as if just switched on.
  void reset();

  // Add one raw ADC sample. Returns true when a block just finished.
  bool addSample(float raw);

  bool toneOn() const { return toneOn_; }
  bool changed() const { return changed_; }  // the last block changed toneOn()

  // Times in ms, counted in blocks from the start, so they follow the sample
  // clock exactly. nowMs() is the end of the last block.
  unsigned long nowMs() const { return blocks_ * blockMs_; }
  unsigned long changeMs() const { return changeBlock_ * blockMs_; }

  // For tuning, all in ADC counts of tone amplitude.
  float strength() const { return strength_; }  // the filter listened to
  float noise() const { return noise_; }
  float onLevel() const;
  float offLevel() const;
  float lockedHz() const { return LOWEST_HZ + locked_ * STEP_HZ; }

 private:
  static int binFor(float hz);
  void updateLock();

  Goertzel bins_[BIN_COUNT];
  unsigned long blockMs_;
  int fixedBin_;
  bool autoMode_ = false;
  bool ignored_ = false;

  float dc_ = 0;
  bool haveDc_ = false;

  int locked_;                       // the filter listened to now
  float average_[BIN_COUNT] = {};    // each filter's strength, smoothed

  float strength_ = 0;
  float noise_ = 0;
  float peak_ = 0;
  bool haveNoise_ = false;

  bool toneOn_ = false;
  bool changed_ = false;
  int agree_ = 0;                    // blocks in a row that want a change
  unsigned long firstAgreeBlock_ = 0;
  unsigned long changeBlock_ = 0;
  unsigned long blocks_ = 0;
};
