#include "mic.h"

#include <Arduino.h>
#include <driver/adc.h>
#include <tone_detector.h>

// GPIO 35 is ADC1 channel 7. ADC1 still works while WiFi is on.
const adc1_channel_t MIC_CHANNEL = ADC1_CHANNEL_7;

const float MIC_SAMPLE_RATE_HZ = 8000;
const int MIC_BLOCK = 80;                // 80 samples at 8 kHz is 10 ms
const float FIXED_TONE_HZ = 600;         // from the timing spec in CLAUDE.md

const int MIC_TIMER = 1;                 // timer 0 is the sidetone
const int TIMER_DIVIDER = 80;            // 80 MHz / 80 = 1 tick per microsecond
const int TIMER_TICKS = 125;             // 125 microseconds is 8000 times a second

const int MIC_CORE = 1;                  // away from WiFi on core 0
const int MIC_PRIORITY = 5;              // above loop() (1), so it is never held up by it
const int QUEUE_BLOCKS = 32;             // 320 ms of blocks, if loop() is slow

// static: these belong to this file only, so their names cannot clash with
// main.cpp.
// The tone detector lives in the mic task and only the task touches it.
// loop() asks for changes through these flags, read by the task each sample.
static ToneDetector detector(MIC_SAMPLE_RATE_HZ, MIC_BLOCK, FIXED_TONE_HZ);
static volatile bool ignoredWanted = false;
static volatile bool autoWanted = false;
static volatile bool resetWanted = false;

static TaskHandle_t micTaskHandle = nullptr;
static QueueHandle_t blockQueue = nullptr;
static hw_timer_t* micTimer = nullptr;

// Counted by the task, copied out once a second for micStats().
static volatile MicStats lastStats = {0, 0, 0};

// Runs 8000 times a second. It only wakes the task.
static void IRAM_ATTR onMicTimer() {
  BaseType_t wokeHigher = pdFALSE;
  vTaskNotifyGiveFromISR(micTaskHandle, &wokeHigher);
  if (wokeHigher) portYIELD_FROM_ISR();
}

static void micTask(void*) {
  uint32_t samples = 0;
  uint32_t longestUs = 0;
  uint32_t missed = 0;
  uint32_t secondStartMs = millis();

  for (;;) {
    // Sleep until the timer wakes us. The count is how many ticks came
    // since last time: more than 1 means some were missed.
    uint32_t ticks = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (ticks > 1) missed += ticks - 1;

    uint32_t startUs = micros();
    if (resetWanted) {
      detector.reset();
      resetWanted = false;
    }
    detector.setAutoMode(autoWanted);
    detector.setIgnored(ignoredWanted);

    int raw = adc1_get_raw(MIC_CHANNEL);
    if (detector.addSample(raw)) {
      MicBlock block = {detector.toneOn(),   detector.changed(),  (uint32_t)detector.nowMs(),
                        (uint32_t)detector.changeMs(), detector.strength(), detector.noise(),
                        detector.onLevel(),  detector.offLevel(), detector.lockedHz()};
      xQueueSend(blockQueue, &block, 0);  // 0: never wait. If full, the block is dropped.
    }

    uint32_t tookUs = micros() - startUs;
    if (tookUs > longestUs) longestUs = tookUs;
    samples++;

    if (millis() - secondStartMs >= 1000) {
      secondStartMs += 1000;
      lastStats.samples = samples;
      lastStats.longestUs = longestUs;
      lastStats.missed = missed;
      samples = 0;
      longestUs = 0;
      missed = 0;
    }
  }
}

void micBegin() {
  // Set up the channel once here. analogRead() would redo the pin set up on
  // every call, 8000 times a second.
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(MIC_CHANNEL, ADC_ATTEN_DB_12);  // 0 to about 3.1 V
  // Keep the ADC switched on. Otherwise each reading switches it on and off.
  adc_power_acquire();

  blockQueue = xQueueCreate(QUEUE_BLOCKS, sizeof(MicBlock));
  xTaskCreatePinnedToCore(micTask, "mic", 4096, nullptr, MIC_PRIORITY, &micTaskHandle, MIC_CORE);

  micTimer = timerBegin(MIC_TIMER, TIMER_DIVIDER, true);
  timerAttachInterrupt(micTimer, &onMicTimer, false);  // false: level, the only kind this core supports
  timerAlarmWrite(micTimer, TIMER_TICKS, true);      // true: repeat forever
  // Not enabled yet: sampling starts with micStart().
}

void micStart() {
  resetWanted = true;
  xQueueReset(blockQueue);
  timerAlarmEnable(micTimer);
}

void micStop() {
  timerAlarmDisable(micTimer);
}

bool micReceive(MicBlock* block) {
  return xQueueReceive(blockQueue, block, 0) == pdTRUE;
}

void micSetIgnored(bool ignored) { ignoredWanted = ignored; }

void micSetAutoMode(bool on) { autoWanted = on; }

bool micAutoMode() { return autoWanted; }

MicStats micStats() {
  MicStats s = {lastStats.samples, lastStats.longestUs, lastStats.missed};
  return s;
}
