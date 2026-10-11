// An iambic keyer: turns two paddles (dit and dah) into perfectly timed
// dits, dahs and gaps. It has no Arduino code in it, so the unit tests can run
// on the Mac too. It does not care where the paddle states come from: touch
// pads, microswitches or a test.
//
// Timing, from CLAUDE.md: 1 unit = 1200 / WPM ms. A dit is 1 unit, a dah is
// 3 units, and after every dit or dah the key stays up for 1 unit.
//
// Rules, checked at the end of each 1 unit gap to pick what comes next:
//   1. Both paddles held (a squeeze): the opposite of the last element, so a
//      squeeze gives dit dah dit dah ...
//   2. A remembered press (memory), the opposite element first.
//   3. One paddle held: that element again.
//   4. Mode B only: if both paddles were held at some point during the last
//      element, and now both are released, one extra opposite element.
//   5. Otherwise stop and wait.
// Memory: a new press of either paddle while an element or its gap is
// running is remembered, so a quick tap is never lost.

#pragma once

enum KeyerMode { KEYER_MODE_A, KEYER_MODE_B };

class IambicKeyer {
 public:
  explicit IambicKeyer(float wpm);

  // Takes effect at the next dit or dah, never halfway through one.
  void setWpm(float wpm);
  float wpm() const { return wpm_; }
  float unitMs() const { return 1200.0f / wpm_; }

  void setMode(KeyerMode mode) { mode_ = mode; }
  KeyerMode mode() const { return mode_; }

  // Feed the paddle states as often as possible, with the time now. Returns
  // true when the key line just changed, see keyDown() and changeMs().
  // At most one change per call, so call it again while it returns true.
  // That way, if a call comes late, no change is lost.
  bool update(bool ditPressed, bool dahPressed, unsigned long nowMs);

  bool keyDown() const { return keyDown_; }

  // When the change should have happened, to the ms. If update() is called
  // a few ms late, this time is still exact.
  unsigned long changeMs() const { return changeMs_; }

  // True while sending an element or its gap.
  bool busy() const { return state_ != IDLE; }

 private:
  enum State { IDLE, ELEMENT, GAP };
  enum Element { NONE, DIT, DAH };

  Element chooseNext(bool dit, bool dah) const;
  void startElement(Element e, unsigned long atMs, bool dit, bool dah);

  float wpm_;
  KeyerMode mode_ = KEYER_MODE_A;

  State state_ = IDLE;
  Element last_ = NONE;          // the element sent last
  unsigned long endMs_ = 0;      // when the current element or gap ends
  unsigned long gapMs_ = 0;      // 1 unit, fixed when the element started

  bool ditMemory_ = false;
  bool dahMemory_ = false;
  bool squeezed_ = false;        // both held at some point this element
  bool lastDit_ = false;         // paddle states at the last call, to spot
  bool lastDah_ = false;         // a new press

  bool keyDown_ = false;
  unsigned long changeMs_ = 0;
};
