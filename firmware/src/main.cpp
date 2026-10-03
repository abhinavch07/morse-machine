// Phase 2, Session 2, Step 14: decode key presses into letters on serial.
// While the button on GPIO 27 is held, a 600 Hz tone with a 5 ms fade in and
// fade out plays on GPIO 25 (DAC1), and both LEDs are on. The press and gap
// times go to the decoder. Letters print on one line as they are decoded, a
// word gap prints a space, and 3 seconds with no presses ends the line.

#include <Arduino.h>
#include <soc/rtc_io_reg.h>
#include <key_decoder.h>
#include <morse.h>
#include <sidetone.h>

// Pins, from the pin map in CLAUDE.md and docs/hardware-notes.md
const int ONBOARD_LED_PIN = 2;           // the blue LED built into the DevKit V1
const int LED_PIN = 26;                  // external LED on the breadboard
const int KEY_PIN = 27;                  // button to GND, INPUT_PULLUP
const int SIDETONE_PIN = 25;             // DAC1, to PAM8403 input L

// Sidetone, from the timing spec in CLAUDE.md
const float SIDETONE_HZ = 600;
const float RAMP_MS = 5;                 // fade in and fade out, stops clicks

// Tone colour: TONE_SOFT (pure sine), TONE_BRIGHT or TONE_SHARP.
const ToneShape TONE_PRESET = TONE_SHARP;

// A hardware timer asks for one new DAC value 40,000 times a second.
const int SAMPLE_RATE_HZ = 40000;
const int SAMPLE_TIMER = 0;              // one of the 4 hardware timers
const int TIMER_DIVIDER = 80;            // 80 MHz / 80 = 1 tick per microsecond
const int TIMER_TICKS = 1000000 / SAMPLE_RATE_HZ;  // 25 microseconds per sample

// A reading must stay the same this long before we believe it.
const unsigned long DEBOUNCE_MS = 10;

// Decoder limits in ms, fitted to the owner's hand (timing test, 3 Oct 2026).
// Presses are about 15 WPM, but gaps are longer, so each limit is set alone.
const float DIT_DAH_SPLIT_MS = 160;      // 2 units at 15 WPM. Dits up to 139, dahs from 180
const float LETTER_GAP_MS = 250;         // gaps inside a letter up to 136, between letters from 427
const float WORD_GAP_MS = 2500;          // gaps between letters up to 1458, pause between groups 3822

// After this long with the key up, start a new line on serial.
const unsigned long NEW_LINE_MS = 5000;

// Temporary: print "down 85" and "up 120" for every press and gap, in ms, so
// we can see how long they really are. Set to false to hide them.
const bool DEBUG_TIMING = false;

// Kept for sendText(), which is not used right now but will be later.
const float WPM = 5;
const char* MESSAGE = "VU";

// Key state, kept between passes of loop().
int lastReading = HIGH;                  // raw pin value from the last pass
unsigned long lastChangeMs = 0;          // when the raw value last changed
bool keyDown = false;                    // the cleaned up, debounced state
unsigned long keyDownStartMs = 0;        // when the current press began
unsigned long keyUpStartMs = 0;          // when the current gap began

KeyDecoder decoder(DIT_DAH_SPLIT_MS, LETTER_GAP_MS, WORD_GAP_MS);
bool lineHasText = false;                // printed something since the last new line

// The only thing loop() and the timer interrupt both touch. volatile tells
// the compiler it can change at any moment, so it must read it fresh every time.
volatile bool toneOn = false;

// Set up once in setup() before the timer starts, then used only by the
// interrupt, so it does not need to be volatile. As a global it lives in RAM.
SidetoneGenerator sidetone;
hw_timer_t* sampleTimer = nullptr;

// Put a value straight into the DAC1 output register: 8 bits starting at
// bit 19. This is what dac_output_voltage() does in the end, without the lock
// and the checks, and it is inline so it ends up in IRAM with its caller.
inline void IRAM_ATTR writeDac1(uint8_t value) {
  SET_PERI_REG_BITS(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_DAC, value, RTC_IO_PDAC1_DAC_S);
}

// Runs 40,000 times a second, in between whatever loop() is doing. It must be
// short and use whole numbers only, so all the maths is in the tables.
void IRAM_ATTR onSampleTimer() {
  writeDac1(sidetone.nextSample(toneOn));
}

// Both LEDs always switch together, so they show the same Morse.
void setLeds(bool on) {
  digitalWrite(ONBOARD_LED_PIN, on ? HIGH : LOW);
  digitalWrite(LED_PIN, on ? HIGH : LOW);
}

// Only sets a flag. The timer interrupt does the fade in or fade out.
void setSidetone(bool on) {
  toneOn = on;
}

// Turn the LEDs on or off and hold them for a time. delay() only takes whole
// milliseconds, so the float time is rounded.
void keyLed(bool on, float ms) {
  setLeds(on);
  delay(lroundf(ms));
}

// Blink one Morse code such as "...-". No gap after the last symbol, because
// the gap that follows depends on what comes next.
void sendCode(const char* code, const MorseTiming& t) {
  for (int i = 0; code[i] != '\0'; i++) {
    if (i > 0) keyLed(false, t.intraGap);
    keyLed(true, code[i] == '.' ? t.dit : t.dah);
  }
  setLeds(false);
}

// Print what the decoder just finished, on the same line: the letter text
// ("K", "<AR>" or "*") for a letter, and one space for a word gap.
void printDecoded(const DecodeResult& r) {
  if (r.letterDone) {
    Serial.print(r.letter);
    lineHasText = true;
  }
  if (r.wordDone) Serial.print(' ');
}

// One debug line like "down 85". Debug lines go on their own line, so if a
// letter is waiting on the current line, end that line first.
void printTiming(const char* label, unsigned long ms) {
  if (!DEBUG_TIMING) return;
  if (lineHasText) {
    Serial.println();
    lineHasText = false;
  }
  Serial.print(label);
  Serial.print(' ');
  Serial.println(ms);
}

// Send a whole text. Same gap rules as totalDurationMs() in the library.
// Not called in this step. Example: sendText(MESSAGE, computeTiming(WPM))
void sendText(const char* text, const MorseTiming& t) {
  bool sentAny = false;
  bool spaceSeen = false;

  for (int i = 0; text[i] != '\0'; i++) {
    if (text[i] == ' ') {
      spaceSeen = true;
      continue;
    }
    const char* code = morseCode(text[i]);
    if (code == nullptr) continue;

    if (sentAny) keyLed(false, spaceSeen ? t.wordGap : t.charGap);

    // Print first, so the text shows up as the letter starts to blink.
    Serial.print((char)toupper(text[i]));
    Serial.print(' ');
    Serial.println(code);

    sendCode(code, t);
    sentAny = true;
    spaceSeen = false;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);  // give the Mac a moment to open the serial monitor
  Serial.println("Hello from ESP32, Morse machine starting");

  pinMode(ONBOARD_LED_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(KEY_PIN, INPUT_PULLUP);

  // The DAC starts at 0 V, but silence is the middle level, 128. Jumping
  // straight there would thump the speaker once, so slide up over 0.5 s.
  for (int level = 0; level <= DAC_MID; level++) {
    dacWrite(SIDETONE_PIN, level);  // also switches the DAC on, setup only
    delay(4);
  }

  // Builds the wave table once, here, never inside the interrupt.
  sidetone.begin(SIDETONE_HZ, SAMPLE_RATE_HZ, RAMP_MS, TONE_PRESET);
  sampleTimer = timerBegin(SAMPLE_TIMER, TIMER_DIVIDER, true);
  timerAttachInterrupt(sampleTimer, &onSampleTimer, false);  // false: level, the only kind this core supports
  timerAlarmWrite(sampleTimer, TIMER_TICKS, true);  // true: repeat forever
  timerAlarmEnable(sampleTimer);
}

// No delay() here, so loop() runs thousands of times a second and never
// misses a press.
void loop() {
  int reading = digitalRead(KEY_PIN);
  unsigned long now = millis();

  // Any change, even a bounce, restarts the quiet time.
  if (reading != lastReading) {
    lastReading = reading;
    lastChangeMs = now;
  }

  // Quiet for DEBOUNCE_MS, so this reading is real.
  if (now - lastChangeMs >= DEBOUNCE_MS) {
    bool pressed = (reading == LOW);  // LOW means pressed, because of the pull-up
    if (pressed != keyDown) {
      keyDown = pressed;
      setLeds(keyDown);
      setSidetone(keyDown);
      // Both ends are seen 10 ms late, so the delays cancel out.
      if (keyDown) {
        printDecoded(decoder.keyUp(now - keyUpStartMs));  // the gap just ended
        printTiming("up", now - keyUpStartMs);
        keyDownStartMs = now;
      } else {
        decoder.keyDown(now - keyDownStartMs);
        printTiming("down", now - keyDownStartMs);
        keyUpStartMs = now;
      }
    }
  }

  // While the key is up, tell the decoder how long the gap is so far, so a
  // letter prints as soon as the gap reaches LETTER_GAP_MS, without waiting for
  // the next press.
  if (!keyDown) {
    printDecoded(decoder.keyUp(now - keyUpStartMs));

    // A long pause ends the line, but never prints empty lines.
    if (lineHasText && now - keyUpStartMs >= NEW_LINE_MS) {
      Serial.println();
      lineHasText = false;
    }
  }
}
