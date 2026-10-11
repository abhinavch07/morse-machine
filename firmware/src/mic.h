// The microphone on GPIO 35, sampled 8000 times a second in its own task.
//
// A hardware timer (timer 1) ticks every 125 microseconds. Its interrupt only
// wakes the mic task, because reading the ADC takes too long and needs a
// lock, which an interrupt must not wait for. The mic task reads one sample
// and feeds the tone detector. Every 10 ms block goes to loop() through a
// queue. The task runs on core 1 with a higher priority than loop(), so the
// sampling stays steady. Core 0 is busy with WiFi, which could hold a sample
// back by milliseconds.
// The sidetone uses timer 0 and the DAC. This uses timer 1 and ADC1. They
// are separate hardware and do not get in each other's way.
// Every function here returns at once.

#pragma once

#include <stdint.h>

// One 10 ms block from the tone detector. Times are in ms from the sample
// clock, which counts 10 ms per block exactly.
struct MicBlock {
  bool toneOn;
  bool changed;        // toneOn changed in this block, at changeMs
  uint32_t ms;         // the end of this block
  uint32_t changeMs;
  float strength;      // tuning numbers, all in ADC counts of tone amplitude
  float noise;
  float onLevel;
  float offLevel;
  float hz;            // the filter listened to
};

// Sampling over the last full second, to check the timing on the board.
struct MicStats {
  uint32_t samples;      // should be close to 8000
  uint32_t longestUs;    // the longest time to take and process one sample
  uint32_t missed;       // timer ticks the task was too busy to answer
};

// Sets up the ADC, the timer and the task, with sampling stopped. Call once
// in setup().
void micBegin();

// Start or stop sampling. Starting also resets the tone detector.
void micStart();
void micStop();

// Takes one block from the queue, if there is one.
bool micReceive(MicBlock* block);

// While ignored, the detector holds the tone off (my own sidetone is playing).
void micSetIgnored(bool ignored);

void micSetAutoMode(bool on);
bool micAutoMode();

MicStats micStats();
