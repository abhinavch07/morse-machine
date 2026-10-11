#include "speed_tracker.h"

static const float GROUP_JUMP = 2.0f;    // dahs are about 3 x dits, never under 2 x
static const float SHORTEST_DIT = 24;    // 50 WPM
static const float LONGEST_DIT = 240;    // 5 WPM

SpeedTracker::SpeedTracker(float startDitMs) : startDitMs_(startDitMs), ditMs_(startDitMs) {}

void SpeedTracker::reset() {
  ditMs_ = startDitMs_;
  count_ = 0;
  next_ = 0;
}

void SpeedTracker::addTone(float ms) {
  tones_[next_] = ms;
  next_ = (next_ + 1) % WINDOW;
  if (count_ < WINDOW) count_++;

  // Sort a copy, shortest first. 8 numbers, so a simple insertion sort.
  float sorted[WINDOW];
  for (int i = 0; i < count_; i++) {
    float v = tones_[i];
    int j = i;
    while (j > 0 && sorted[j - 1] > v) {
      sorted[j] = sorted[j - 1];
      j--;
    }
    sorted[j] = v;
  }

  // The biggest jump between neighbours: the last dit is at splitAt.
  int splitAt = -1;
  float biggest = GROUP_JUMP;
  for (int i = 0; i + 1 < count_; i++) {
    float jump = sorted[i + 1] / sorted[i];
    if (jump >= biggest) {
      biggest = jump;
      splitAt = i;
    }
  }

  float total = 0;
  for (int i = 0; i < count_; i++) total += sorted[i];

  float dit;
  if (splitAt >= 0) {
    float dits = 0;
    for (int i = 0; i <= splitAt; i++) dits += sorted[i];
    float dahs = total - dits;
    dit = (dits + dahs / 3) / count_;
  } else {
    float average = total / count_;
    dit = (average < 2 * ditMs_) ? average : average / 3;
  }

  if (dit < SHORTEST_DIT) dit = SHORTEST_DIT;
  if (dit > LONGEST_DIT) dit = LONGEST_DIT;
  ditMs_ = dit;
}
