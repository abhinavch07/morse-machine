#include "tone_detector.h"

// Tuning numbers. Ratios compare amplitudes, so 4 is 12 dB above the noise.
static const float ON_ABOVE_NOISE = 4.0f;    // never on below 4 x noise
static const float OFF_ABOVE_NOISE = 2.0f;   // always off below 2 x noise
static const float ON_FRACTION = 0.5f;       // on above halfway from noise to peak
static const float OFF_FRACTION = 0.25f;     // off below a quarter of the way
static const float LOWEST_NOISE = 1.0f;      // a perfectly silent input still has limits
static const float DC_SPEED = 1.0f / 256;    // follows DC drift in about 30 ms
// The noise level falls fast but rises slowly, over about half a second. A
// loud tone leaks a little into every filter, and this stops one long dah
// from pushing the noise level up much. Real louder noise still gets through.
static const float NOISE_FALL = 0.2f;
static const float NOISE_RISE = 0.02f;
// A Morse tone lights up only a few filters, noise lights up all of them. So
// the 3rd weakest of the 11 filters shows the background, even during a
// tone. On white noise it reads 1 / 1.64 of the listening filter's average
// (measured on the Mac, 20,000 blocks), so it is scaled back up by 1.64.
static const int NOISE_RANK = 2;             // 0 is the weakest
static const float NOISE_SCALE = 1.64f;
static const float PEAK_FOLLOW = 0.05f;      // peak follows a fading tone
static const float PEAK_DECAY = 0.005f;      // between tones, peak sinks over about 2 s
static const float AUTO_SMOOTH = 0.2f;       // how fast each filter's average moves
static const float AUTO_SWITCH = 1.5f;       // a new filter must be 1.5 x stronger
static const int BLOCKS_TO_AGREE = 2;

// The NOISE_RANK weakest of the filters' strengths. Sorting 11 numbers by
// hand is quick, and there is no std::sort on every compiler we use.
static float weakest(const Goertzel* bins, int count, int rank) {
  float sorted[ToneDetector::BIN_COUNT];
  for (int i = 0; i < count; i++) {
    float v = bins[i].strength();
    int j = i;
    while (j > 0 && sorted[j - 1] > v) {
      sorted[j] = sorted[j - 1];
      j--;
    }
    sorted[j] = v;
  }
  return sorted[rank];
}

ToneDetector::ToneDetector(float sampleRateHz, int blockSize, float fixedHz)
    : bins_{
          {400, sampleRateHz, blockSize}, {450, sampleRateHz, blockSize},
          {500, sampleRateHz, blockSize}, {550, sampleRateHz, blockSize},
          {600, sampleRateHz, blockSize}, {650, sampleRateHz, blockSize},
          {700, sampleRateHz, blockSize}, {750, sampleRateHz, blockSize},
          {800, sampleRateHz, blockSize}, {850, sampleRateHz, blockSize},
          {900, sampleRateHz, blockSize}},
      blockMs_((unsigned long)(blockSize * 1000.0f / sampleRateHz + 0.5f)),
      fixedBin_(binFor(fixedHz)),
      locked_(fixedBin_) {}

int ToneDetector::binFor(float hz) {
  int bin = (int)((hz - LOWEST_HZ) / STEP_HZ + 0.5f);
  if (bin < 0) return 0;
  if (bin >= BIN_COUNT) return BIN_COUNT - 1;
  return bin;
}

void ToneDetector::setAutoMode(bool on) {
  if (on == autoMode_) return;
  autoMode_ = on;
  if (!on) locked_ = fixedBin_;
}

void ToneDetector::reset() {
  haveDc_ = false;
  haveNoise_ = false;
  toneOn_ = false;
  changed_ = false;
  agree_ = 0;
  locked_ = fixedBin_;
  for (int i = 0; i < BIN_COUNT; i++) average_[i] = 0;
  // blocks_ keeps counting, so times never go backwards.
}

float ToneDetector::onLevel() const {
  float byNoise = noise_ * ON_ABOVE_NOISE;
  float byPeak = noise_ + ON_FRACTION * (peak_ - noise_);
  return byNoise > byPeak ? byNoise : byPeak;
}

float ToneDetector::offLevel() const {
  float byNoise = noise_ * OFF_ABOVE_NOISE;
  float byPeak = noise_ + OFF_FRACTION * (peak_ - noise_);
  return byNoise > byPeak ? byNoise : byPeak;
}

// Auto mode: move to another filter only when it has been clearly stronger
// for a while, and is loud enough to be a real tone, not just noise.
void ToneDetector::updateLock() {
  int best = 0;
  for (int i = 0; i < BIN_COUNT; i++) {
    average_[i] += (bins_[i].strength() - average_[i]) * AUTO_SMOOTH;
    if (average_[i] > average_[best]) best = i;
  }
  if (!autoMode_ || best == locked_) return;
  if (average_[best] > average_[locked_] * AUTO_SWITCH &&
      average_[best] > noise_ * ON_ABOVE_NOISE) {
    locked_ = best;
  }
}

bool ToneDetector::addSample(float raw) {
  if (!haveDc_) {
    dc_ = raw;
    haveDc_ = true;
  }
  dc_ += (raw - dc_) * DC_SPEED;
  float x = raw - dc_;

  // All filters run in step, so they all finish a block together.
  bool blockDone = false;
  for (int i = 0; i < BIN_COUNT; i++) blockDone = bins_[i].addSample(x);
  if (!blockDone) return false;

  unsigned long block = blocks_++;
  changed_ = false;

  // The noise level first, because picking a filter in auto mode needs it.
  float blockNoise = weakest(bins_, BIN_COUNT, NOISE_RANK) * NOISE_SCALE;
  if (!haveNoise_) {
    noise_ = blockNoise > LOWEST_NOISE ? blockNoise : LOWEST_NOISE;
    peak_ = noise_;
    haveNoise_ = true;
  }
  updateLock();
  strength_ = bins_[locked_].strength();

  if (ignored_) {
    agree_ = 0;
    if (toneOn_) {
      toneOn_ = false;
      changed_ = true;
      changeBlock_ = block;
    }
    return true;
  }

  bool wantOn = toneOn_ ? (strength_ > offLevel()) : (strength_ > onLevel());
  if (wantOn != toneOn_) {
    if (agree_ == 0) firstAgreeBlock_ = block;
    if (++agree_ >= BLOCKS_TO_AGREE) {
      toneOn_ = wantOn;
      changed_ = true;
      changeBlock_ = firstAgreeBlock_;
      agree_ = 0;
    }
  } else {
    agree_ = 0;
  }

  if (toneOn_) {
    // Jump up to a louder tone fast, follow a fading one slowly.
    float speed = strength_ > peak_ ? 0.5f : PEAK_FOLLOW;
    peak_ += (strength_ - peak_) * speed;
  } else {
    peak_ -= (peak_ - noise_) * PEAK_DECAY;
  }

  noise_ += (blockNoise - noise_) * (blockNoise < noise_ ? NOISE_FALL : NOISE_RISE);
  if (noise_ < LOWEST_NOISE) noise_ = LOWEST_NOISE;
  if (peak_ < noise_) peak_ = noise_;
  return true;
}
