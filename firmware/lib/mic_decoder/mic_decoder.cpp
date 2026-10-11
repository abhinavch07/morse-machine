#include "mic_decoder.h"

static const float SPLIT_DITS = 2;
static const float LETTER_GAP_DITS = 2;
static const float WORD_GAP_DITS = 5;

static const DecodeResult NOTHING = {false, false, nullptr, nullptr};

MicDecoder::MicDecoder(float startWpm)
    : speed_(1200.0f / startWpm), decoder_(0, 0, 0) {
  applyLimits();
}

void MicDecoder::applyLimits() {
  float dit = speed_.ditMs();
  decoder_.setLimits(SPLIT_DITS * dit, LETTER_GAP_DITS * dit, WORD_GAP_DITS * dit);
}

DecodeResult MicDecoder::toneChanged(bool on, unsigned long atMs) {
  if (on == toneOn_) return NOTHING;
  unsigned long length = atMs - lastChangeMs_;
  toneOn_ = on;
  lastChangeMs_ = atMs;

  if (on) {
    // A gap just ended. Before the first tone there is nothing to finish.
    if (!started_) {
      started_ = true;
      return NOTHING;
    }
    return decoder_.keyUp(length);
  }

  // A tone just ended. Learn from it first, so it is judged by the newest
  // speed, then add it to the letter.
  speed_.addTone(length);
  applyLimits();
  decoder_.keyDown(length);
  return NOTHING;
}

DecodeResult MicDecoder::update(unsigned long nowMs) {
  if (toneOn_ || !started_) return NOTHING;
  return decoder_.keyUp(nowMs - lastChangeMs_);
}

unsigned long MicDecoder::quietMs(unsigned long nowMs) const {
  return toneOn_ ? 0 : nowMs - lastChangeMs_;
}

void MicDecoder::reset() {
  speed_.reset();
  applyLimits();
  // A long gap finishes any letter in progress, so the old dits and dahs
  // cannot join the first new letter.
  if (started_) decoder_.keyUp(1e9f);
  toneOn_ = false;
  started_ = false;
}
