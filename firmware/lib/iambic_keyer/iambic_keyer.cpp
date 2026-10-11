#include "iambic_keyer.h"

#include <math.h>

IambicKeyer::IambicKeyer(float wpm) : wpm_(wpm) {}

void IambicKeyer::setWpm(float wpm) {
  if (wpm > 0) wpm_ = wpm;
}

// True once nowMs has reached atMs. Written as a difference so it still
// works when millis() wraps back to 0 after 49 days.
static bool reached(unsigned long nowMs, unsigned long atMs) {
  return (long)(nowMs - atMs) >= 0;
}

IambicKeyer::Element IambicKeyer::chooseNext(bool dit, bool dah) const {
  Element opposite = (last_ == DIT) ? DAH : DIT;

  if (dit && dah) return (last_ == NONE) ? DIT : opposite;

  bool oppositeMemory = (opposite == DIT) ? ditMemory_ : dahMemory_;
  if (oppositeMemory) return opposite;
  if (ditMemory_) return DIT;
  if (dahMemory_) return DAH;

  if (dit) return DIT;
  if (dah) return DAH;

  if (mode_ == KEYER_MODE_B && squeezed_ && last_ != NONE) return opposite;
  return NONE;
}

void IambicKeyer::startElement(Element e, unsigned long atMs, bool dit, bool dah) {
  // Whole ms, because millis() counts whole ms. At 8 to 25 WPM rounding
  // changes a unit by less than 0.5 ms.
  unsigned long unit = lroundf(unitMs());

  keyDown_ = true;
  changeMs_ = atMs;
  endMs_ = atMs + (e == DAH ? 3 * unit : unit);
  gapMs_ = unit;
  state_ = ELEMENT;
  last_ = e;

  // This element is the one that was remembered, so forget it now.
  if (e == DIT) ditMemory_ = false;
  if (e == DAH) dahMemory_ = false;
  squeezed_ = dit && dah;
}

bool IambicKeyer::update(bool dit, bool dah, unsigned long nowMs) {
  bool newDit = dit && !lastDit_;
  bool newDah = dah && !lastDah_;
  lastDit_ = dit;
  lastDah_ = dah;

  // While busy, remember new presses and notice a squeeze.
  if (state_ != IDLE) {
    if (newDit) ditMemory_ = true;
    if (newDah) dahMemory_ = true;
    if (dit && dah) squeezed_ = true;
  }

  switch (state_) {
    case IDLE: {
      Element next = chooseNext(dit, dah);
      if (next == NONE) return false;
      startElement(next, nowMs, dit, dah);
      return true;
    }

    case ELEMENT:
      if (!reached(nowMs, endMs_)) return false;
      keyDown_ = false;
      changeMs_ = endMs_;
      endMs_ += gapMs_;
      state_ = GAP;
      return true;

    case GAP: {
      if (!reached(nowMs, endMs_)) return false;
      Element next = chooseNext(dit, dah);
      if (next == NONE) {
        // Back to waiting. last_ goes too, so the next press starts fresh
        // and a squeeze from rest always starts with a dit.
        state_ = IDLE;
        last_ = NONE;
        squeezed_ = false;
        return false;
      }
      // Start exactly when the gap ended, not when we noticed, so the
      // rhythm stays exact even if this call came a little late.
      startElement(next, endMs_, dit, dah);
      return true;
    }
  }
  return false;
}
