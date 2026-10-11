// Works out a sender's dit length from the lengths of their recent tones. It
// has no Arduino code in it, so the unit tests can run on the Mac too.
//
// It keeps the last 8 tone lengths and sorts them. Dits and dahs form two
// groups, about 3 times apart, so the biggest jump between neighbours in the
// sorted list (at least 2 times) splits dits from dahs. Then:
//   dit length = average of (each dit) and (each dah / 3).
// If there is no such jump, all 8 are the same kind, like "EEEE" or "TTTT".
// Then they count as dits if shorter than 2 of the current dit length, or
// as dahs otherwise.
// Because only the last 8 tones count, a new sender at a new speed takes
// over after a few letters.

#pragma once

class SpeedTracker {
 public:
  explicit SpeedTracker(float startDitMs);

  // A tone of ms just ended.
  void addTone(float ms);

  float ditMs() const { return ditMs_; }
  float wpm() const { return 1200.0f / ditMs_; }

  void reset();

 private:
  static const int WINDOW = 8;

  float startDitMs_;
  float ditMs_;
  float tones_[WINDOW];
  int count_ = 0;   // how many of tones_ are filled
  int next_ = 0;    // where the next tone goes
};
