// Phase 5: receive side, a microphone tone decoder.
// Phase 4 and before: iambic keyer with a touch paddle and a speed knob.
// Two ways to key, both feeding one key line:
//   the button on GPIO 27, a straight key, down while it is held, and
//   the touch paddle, pads on GPIO 32 (dit) and GPIO 33 (dah), through the
//   iambic keyer, which sends perfectly timed dits and dahs.
// The knob on GPIO 34 sets the keyer speed, 8 to 25 WPM. Type a, b, swap, t
// or ? and Enter in the serial monitor to change the keyer mode, swap the
// pads or watch the raw touch readings.
// While the key line is down, a 600 Hz tone with a 5 ms fade in and fade out
// plays on GPIO 25 (DAC1), and both LEDs are on. The press and gap times go
// to the decoder. Letters print on one line as they are decoded, a word gap
// prints a space, and 5 seconds with no presses ends the line.
// Type m to decode Morse heard by the microphone on GPIO 35 (see mic.cpp).
// The mic's text and the tone it locked on to replace the relay text on the
// OLED. auto finds the sender's tone, l prints live levels for tuning.
// Each press and release is also sent to the relay. Key events from other
// devices play at 700 Hz, 200 ms after they were sent, and are decoded too.
// The SH1106 OLED shows my text, the received text, the network status and
// the dits and dahs of the letter being keyed now.
// The network runs in its own task (network.cpp), so keying never waits for it.

#include <Arduino.h>
#include <soc/rtc_io_reg.h>
#include <U8g2lib.h>
#include <ctype.h>
#include <iambic_keyer.h>
#include <key_decoder.h>
#include <morse.h>
#include <playout.h>
#include <screen_text.h>
#include <sidetone.h>
#include <speed_knob.h>
#include <touch_key.h>
#include <string.h>

#include <mic_decoder.h>

#include "mic.h"
#include "network.h"

// Pins, from the pin map in CLAUDE.md and docs/hardware-notes.md
const int ONBOARD_LED_PIN = 2;           // the blue LED built into the DevKit V1
const int LED_PIN = 26;                  // external LED on the breadboard
const int KEY_PIN = 27;                  // button to GND, INPUT_PULLUP
const int SIDETONE_PIN = 25;             // DAC1, to PAM8403 input L
const int DIT_PAD_PIN = 32;              // touch channel T9, left pad, thumb
const int DAH_PAD_PIN = 33;              // touch channel T8, right pad, index finger
const int SPEED_KNOB_PIN = 34;           // pot middle leg, ADC1 so it works with WiFi
const int OLED_SDA_PIN = 21;             // I2C data
const int OLED_SCL_PIN = 22;             // I2C clock

// The JMD1.3A board is set to address 0x78. That is the 8 bit form, which is
// what U8g2 wants. Most I2C scanners show the 7 bit form, 0x3C.
const uint8_t OLED_I2C_ADDRESS = 0x78;
const uint32_t OLED_BUS_HZ = 400000;     // 400 kHz "fast mode" I2C

// Sidetone, from the timing spec in CLAUDE.md
const float SIDETONE_HZ = 600;
const float REMOTE_TONE_HZ = 700;        // other devices, so I can tell them from me
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

// Touch paddle, both pads. The reading drops when the pad is touched. Two limits, so a
// reading that wobbles around one of them cannot flicker the key.
const float TOUCH_PRESS_RATIO = 0.65;    // touched below 65% of the no touch level
const float TOUCH_RELEASE_RATIO = 0.80;  // released above 80%
const int TOUCH_READINGS_TO_AGREE = 3;   // readings in a row before a change
const unsigned long TOUCH_CALIBRATE_MS = 2000;

// The touch hardware measures on its own, over and over, and touchRead()
// just gives the newest result. By default it rests 4096 slow clock cycles
// (150 kHz) between measurements, about 27 ms, far too slow for Morse.
// 256 cycles is about 1.7 ms, plus 0.5 ms to measure (4096 cycles at 8 MHz),
// so a fresh result about every 2.2 ms. We read every 3 ms, so each reading
// is a new one, and 3 agreeing readings take about 6 to 9 ms, close to the
// button's 10 ms debounce. With two pads, if the hardware measures them one
// after the other, it is about 1.7 + 2 x 0.5 = 2.7 ms, still under 3 ms.
const uint16_t TOUCH_MEASURE_CYCLES = 0x1000;
const uint16_t TOUCH_SLEEP_CYCLES = 0x100;
const unsigned long TOUCH_READ_MS = 3;

// Decoder limits in ms, fitted to the owner's hand (timing test, 3 Oct 2026).
// Presses are about 15 WPM, but gaps are longer, so each limit is set alone.
const float DIT_DAH_SPLIT_MS = 160;      // 2 units at 15 WPM. Dits up to 139, dahs from 180
const float LETTER_GAP_MS = 250;         // gaps inside a letter up to 136, between letters from 427
const float WORD_GAP_MS = 2500;          // gaps between letters up to 1458, pause between groups 3822

// Decoder limits for the paddle, in units of the keyer speed. The keyer's
// presses and gaps inside a letter are exact, so the split can sit right
// between them. The word gap is 7 units, not the textbook 5, to forgive slow
// spacing between letters while learning the paddle.
const float KEYER_SPLIT_UNITS = 2;       // dit 1 unit, dah 3 units
const float KEYER_LETTER_GAP_UNITS = 2;  // gap inside a letter is always 1 unit
const float KEYER_WORD_GAP_UNITS = 7;

// Keyer settings at start up. Serial commands change them until the next
// restart.
const KeyerMode KEYER_MODE_AT_START = KEYER_MODE_A;
const bool PADDLES_SWAPPED_AT_START = false;  // true: GPIO 33 is dit, GPIO 32 is dah

// Speed knob. The ADC gives 0 to 4095 for 0 to 3.3 V. If turning the knob up
// makes it slower, the outer legs are the other way round: set this to true.
const bool KNOB_REVERSED = false;
const int KNOB_MIN_WPM = 8;
const int KNOB_MAX_WPM = 25;
const int KNOB_JITTER = 40;              // ADC counts, about 1/6 of one WPM step
const unsigned long KNOB_READ_MS = 10;   // 16 readings fill the average in 160 ms

// After this long with the key up, start a new line on serial.
const unsigned long NEW_LINE_MS = 5000;

// Microphone decoding. The mic is ignored while either sidetone plays, and
// this long after, so the room echo of my own tone dies away first.
const unsigned long MIC_HANGOVER_MS = 50;
const float MIC_START_WPM = 15;          // a first guess, it adapts from the first tones
const unsigned long MIC_LEVELS_MS = 1000;

// Temporary: print "down 85" and "up 120" for every press and gap, in ms, so
// we can see how long they really are. Set to false to hide them.
const bool DEBUG_TIMING = false;

// Touch monitor: print the raw reading of both pads once a second, to check
// what each pad reads. The serial command t turns it on and off.
const bool TOUCH_MONITOR_AT_START = false;
const unsigned long TOUCH_MONITOR_MS = 1000;

// Two separate pads never calibrate to almost the same level. If they do,
// they are probably connected to each other, for example two jumpers in the
// same breadboard row, or foil pads touching.
const float SAME_LEVEL_WARNING = 1.0;

// Print one line for every key event sent to or received from the relay.
// Each line ends the current line of decoded letters, so set this to false
// for clean decoded text.
const bool DEBUG_EVENTS = true;

// Kept for sendText(), which is not used right now but will be later.
const float WPM = 5;
const char* MESSAGE = "VU";

// Key state, kept between passes of loop().
bool buttonDown = false;                 // the button, after debounce
bool keyDown = false;                    // button OR keyer, what we send
unsigned long lastKeyChangeMs = 0;       // time of the last keyDown change
unsigned long keyDownStartMs = 0;        // when the current press began
unsigned long keyUpStartMs = 0;          // when the current gap began

KeyDecoder decoder(DIT_DAH_SPLIT_MS, LETTER_GAP_MS, WORD_GAP_MS);
bool lineHasText = false;                // printed something since the last new line
bool lineIsMic = false;                  // the text on that line came from the mic

// Set by the key pin interrupt at every raw change, bounces included. loop()
// uses this time, not the time it happens to notice the change, so a slow
// screen update cannot make a press or gap look longer or shorter.
volatile unsigned long lastEdgeMs = 0;

TouchKey ditPad(TOUCH_PRESS_RATIO, TOUCH_RELEASE_RATIO, TOUCH_READINGS_TO_AGREE);
TouchKey dahPad(TOUCH_PRESS_RATIO, TOUCH_RELEASE_RATIO, TOUCH_READINGS_TO_AGREE);
unsigned long lastTouchReadMs = 0;
unsigned long lastTouchPrintMs = 0;
bool touchMonitor = TOUCH_MONITOR_AT_START;

// The keyer starts at the slowest speed. setup() reads the knob straight
// away and sets the real speed.
IambicKeyer keyer(KNOB_MIN_WPM);
SpeedKnob knob(KNOB_MIN_WPM, KNOB_MAX_WPM, KNOB_JITTER);
unsigned long lastKnobReadMs = 0;
bool paddlesSwapped = PADDLES_SWAPPED_AT_START;

// Microphone decoding, all in loop(). The sampling is in mic.cpp.
bool micOn = false;
bool micLevels = false;                  // printing levels once a second
MicDecoder micDecoder(MIC_START_WPM);
ScreenText micText(2);                   // replaces receivedText on screen while micOn
MicBlock lastMicBlock = {};
float shownMicHz = 0;
bool shownMicTone = false;
int shownMicWpm = 0;
unsigned long lastSoundMs = 0;           // when either sidetone was last on
unsigned long lastLevelsMs = 0;

// A serial command being typed, like "swap".
char command[16];
int commandLength = 0;

// The screen. F means the whole picture is kept in a 1 KB buffer in RAM, so
// we can draw it all at once and then send it to the screen in pieces.
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE, OLED_SCL_PIN, OLED_SDA_PIN);
ScreenText myText(2);                    // what I key, 2 lines
ScreenText receivedText(2);              // what others key, 2 lines
char lastReceivedFrom[MAX_SENDER_NAME + 1] = "";  // sender of the last received letter
NetStatus shownStatus = NET_NO_WIFI;     // network status on the screen now
bool screenChanged = true;               // draw once at start up

// Key events from other devices: when to play them, and their decoded text.
RemoteKeys remoteKeys(DIT_DAH_SPLIT_MS, LETTER_GAP_MS, WORD_GAP_MS);
int nextPage = -1;                       // next 8 pixel row to send, -1 when idle

// The only things loop() and the timer interrupt both touch. volatile tells
// the compiler they can change at any moment, so it must read them fresh
// every time.
volatile bool toneOn = false;            // my key
volatile bool remoteToneOn = false;      // any other device's key

// Set up once in setup() before the timer starts, then used only by the
// interrupt, so they do not need to be volatile. As globals they live in RAM.
SidetoneGenerator sidetone;              // 600 Hz, my key
SidetoneGenerator remoteTone;            // 700 Hz, other devices
hw_timer_t* sampleTimer = nullptr;

// Put a value straight into the DAC1 output register: 8 bits starting at
// bit 19. This is what dac_output_voltage() does in the end, without the lock
// and the checks, and it is inline so it ends up in IRAM with its caller.
inline void IRAM_ATTR writeDac1(uint8_t value) {
  SET_PERI_REG_BITS(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_DAC, value, RTC_IO_PDAC1_DAC_S);
}

// Runs 40,000 times a second, in between whatever loop() is doing. It must be
// short and use whole numbers only, so all the maths is in the tables.
// Both tones are mixed by adding their swings around the middle level. My
// own tone wins: while my key is down the remote tone fades out. The two
// fades are mirror images, so together they never go past full scale.
void IRAM_ATTR onSampleTimer() {
  int mine = sidetone.nextSample(toneOn) - DAC_MID;
  int theirs = remoteTone.nextSample(remoteToneOn && !toneOn) - DAC_MID;
  int level = DAC_MID + mine + theirs;
  if (level < 0) level = 0;
  if (level > 255) level = 255;
  writeDac1((uint8_t)level);
}

// Runs on every rising or falling edge of the key pin. It only notes the time.
// The debounce in loop() decides later whether the change was real.
void IRAM_ATTR onKeyEdge() {
  lastEdgeMs = millis();
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
    // My letters never join a line of mic text.
    if (lineHasText && lineIsMic) Serial.println();
    lineIsMic = false;
    Serial.print(r.letter);
    lineHasText = true;
  }
  if (r.wordDone && lineHasText && !lineIsMic) Serial.print(' ');
}

// Mic letters go on their own lines, starting with "mic: ".
void printMicDecoded(const DecodeResult& r) {
  if (r.letterDone) {
    if (lineHasText && !lineIsMic) {
      Serial.println();
      lineHasText = false;
    }
    if (!lineHasText) Serial.print("mic: ");
    lineIsMic = true;
    Serial.print(r.letter);
    lineHasText = true;
  }
  if (r.wordDone && lineHasText && lineIsMic) Serial.print(' ');
}

void reportMicDecoded(const DecodeResult& r) {
  printMicDecoded(r);
  if (r.letterDone) micText.addLetter(r.letter);
  if (r.wordDone) micText.addSpace();
  if (r.letterDone || r.wordDone) screenChanged = true;
}

// Same as printDecoded(), but for the screen.
void showDecoded(const DecodeResult& r) {
  if (r.letterDone) myText.addLetter(r.letter);
  if (r.wordDone) myText.addSpace();
  if (r.letterDone || r.wordDone) screenChanged = true;
}

// A letter or word gap from another device. When the sender changes, start a
// new line with their first letter, like "p:" for phone.
void showReceived(const Received& got) {
  bool sameSender = strcmp(got.sender, lastReceivedFrom) == 0;
  if (got.result.letterDone) {
    if (!sameSender) {
      receivedText.newLine();
      char label[3] = {got.sender[0], ':', '\0'};
      receivedText.addLetter(label);
      strncpy(lastReceivedFrom, got.sender, MAX_SENDER_NAME);
    }
    receivedText.addLetter(got.result.letter);
  } else if (got.result.wordDone && sameSender) {
    receivedText.addSpace();
  }
  screenChanged = true;
}

void reportDecoded(const DecodeResult& r) {
  printDecoded(r);
  showDecoded(r);
}

// Draw the whole picture into the RAM buffer. Nothing goes to the screen
// yet, so this is fast, well under a millisecond.
void drawScreen() {
  oled.clearBuffer();

  // Top: a small title, the network status on the right, and a line under.
  oled.setFont(u8g2_font_5x7_tr);
  char title[16];
  snprintf(title, sizeof(title), "%dwpm %c%s", knob.wpm(),
           keyer.mode() == KEYER_MODE_A ? 'A' : 'B', paddlesSwapped ? " sw" : "");
  oled.drawStr(0, 7, title);
  const char* status = shownStatus == NET_RELAY ? "RELAY ok"
                       : shownStatus == NET_WIFI ? "WiFi only"
                                                 : "no WiFi";
  oled.drawStr(128 - oled.getStrWidth(status), 7, status);
  oled.drawHLine(0, 9, 128);

  // Two lines of my text, a line, two lines of received text, 10 pixels apart.
  oled.setFont(u8g2_font_6x10_tr);
  for (int row = 0; row < myText.rows(); row++) {
    oled.drawStr(0, 19 + row * 10, myText.line(row));
  }
  oled.drawHLine(0, 31, 128);
  ScreenText& lower = micOn ? micText : receivedText;
  for (int row = 0; row < lower.rows(); row++) {
    oled.drawStr(0, 41 + row * 10, lower.line(row));
  }

  // Bottom: a line, then the dits and dahs of the letter being keyed now.
  // With the mic on, the mic's tone and speed instead, with * while it hears
  // a tone.
  oled.drawHLine(0, 53, 128);
  char bottom[24];
  if (micOn) {
    snprintf(bottom, sizeof(bottom), "mic %dHz%s %dwpm%s", (int)shownMicHz,
             micAutoMode() ? " auto" : "", shownMicWpm, shownMicTone ? " *" : "");
  } else {
    spacedCode(decoder.currentCode(), bottom);
  }
  oled.drawStr(0, 63, bottom);
}

// Sending the whole buffer over I2C takes about 25 ms, and loop() would stop
// for all of it. So it goes one 8 pixel tall page at a time, one page per
// pass of loop(), which keeps each stop to a few ms. If something changes
// while sending, draw again and start over from the top page.
void updateScreen() {
  if (screenChanged) {
    screenChanged = false;
    drawScreen();
    nextPage = 0;
  }
  if (nextPage < 0) return;

  // Sizes are in tiles of 8 x 8 pixels: 16 across, 8 down.
  oled.updateDisplayArea(0, nextPage, oled.getBufferTileWidth(), 1);
  nextPage++;
  if (nextPage >= oled.getBufferTileHeight()) nextPage = -1;
}

// With DEBUG_TIMING on, print how long the screen takes to update, so the
// numbers in the comments above can be checked on the real board.
void printScreenTiming() {
  if (!DEBUG_TIMING) return;
  unsigned long start = micros();
  oled.sendBuffer();
  unsigned long fullUs = micros() - start;
  start = micros();
  oled.updateDisplayArea(0, 0, oled.getBufferTileWidth(), 1);
  unsigned long pageUs = micros() - start;
  Serial.printf("oled full %lu us, one page %lu us\n", fullUs, pageUs);
}

// One line of its own, like "relay connected". If decoded letters are
// waiting on the current line, end that line first.
void printLine(const char* text) {
  if (lineHasText) {
    Serial.println();
    lineHasText = false;
  }
  Serial.println(text);
}

// One debug line like "down 85".
void printDebug(const char* label, unsigned long value) {
  char line[48];
  snprintf(line, sizeof(line), "%s %lu", label, value);
  printLine(line);
}

void printTiming(const char* label, unsigned long ms) {
  if (DEBUG_TIMING) printDebug(label, ms);
}

void printPadLevels(const char* name, int pin, const TouchKey& pad) {
  Serial.printf("Touch %s pad GPIO %d: no touch level %.1f, touched below %.1f, released above %.1f\n",
                name, pin, pad.noTouchLevel(), pad.pressLevel(), pad.releaseLevel());
}

// Learn each pad's "no touch" level. It changes with humidity, the length of
// the wire and the size of the pad, so it is measured fresh at every start.
// Both pads are measured in the same 2 seconds, each on its own.
void calibrateTouch() {
  // The first call for each pin switches it on. The touch hardware as a
  // whole is only switched on once, by the very first call.
  touchRead(DIT_PAD_PIN);
  touchRead(DAH_PAD_PIN);
  // Must come after those first calls. Setting the cycles before them would
  // be undone, with the two numbers swapped, when the hardware is switched on.
  touchSetCycles(TOUCH_MEASURE_CYCLES, TOUCH_SLEEP_CYCLES);
  delay(50);  // let a few measurements finish at the new speed

  Serial.println("Touch calibrating, do not touch the pads");
  unsigned long start = millis();
  while (millis() - start < TOUCH_CALIBRATE_MS) {
    ditPad.addCalibrationReading(touchRead(DIT_PAD_PIN));
    dahPad.addCalibrationReading(touchRead(DAH_PAD_PIN));
    delay(TOUCH_READ_MS);
  }
  ditPad.finishCalibration();
  dahPad.finishCalibration();

  printPadLevels("left", DIT_PAD_PIN, ditPad);
  printPadLevels("right", DAH_PAD_PIN, dahPad);
  if (fabsf(ditPad.noTouchLevel() - dahPad.noTouchLevel()) < SAME_LEVEL_WARNING) {
    Serial.println("Warning: both pads read almost the same. Check that GPIO 32 and GPIO 33");
    Serial.println("are not connected together (same breadboard row, or pads touching).");
  }
}

// The straight key keeps the limits fitted to my hand. The paddle uses
// limits that follow the keyer speed.
void useDecoderLimits(bool forKeyer) {
  if (forKeyer) {
    float unit = keyer.unitMs();
    decoder.setLimits(KEYER_SPLIT_UNITS * unit, KEYER_LETTER_GAP_UNITS * unit,
                      KEYER_WORD_GAP_UNITS * unit);
  } else {
    decoder.setLimits(DIT_DAH_SPLIT_MS, LETTER_GAP_MS, WORD_GAP_MS);
  }
}

// Called when the button or the keyer changes. keyDown is "button OR keyer",
// so it only changes when the first one goes down or the last one comes up.
// atMs is when that input really changed, not when loop() noticed.
void updateKey(unsigned long atMs) {
  bool down = buttonDown || keyer.keyDown();
  if (down == keyDown) return;

  // The button and the keyer are noticed with different delays, so in a rare
  // overlap atMs could be a little before the last change. Never go back in
  // time, or a press or gap would come out negative.
  if ((long)(atMs - lastKeyChangeMs) < 0) atMs = lastKeyChangeMs;
  lastKeyChangeMs = atMs;

  keyDown = down;
  setLeds(keyDown);
  setSidetone(keyDown);

  // Send it with the time it really happened. If the relay is not connected
  // the event is dropped, and everything else here carries on as normal.
  if (networkSend(keyDown, atMs) && DEBUG_EVENTS) {
    printDebug(keyDown ? "send down" : "send up", atMs);
  }

  if (keyDown) {
    reportDecoded(decoder.keyUp(atMs - keyUpStartMs));  // the gap just ended
    printTiming("up", atMs - keyUpStartMs);
    // Only now switch limits for this press. The gap above belonged to the
    // letter before, so it was judged by that letter's limits.
    useDecoderLimits(keyer.keyDown());
    keyDownStartMs = atMs;
  } else {
    decoder.keyDown(atMs - keyDownStartMs);
    printTiming("down", atMs - keyDownStartMs);
    keyUpStartMs = atMs;
    screenChanged = true;  // one more dit or dah on the bottom line
  }
}

// Read both pads every TOUCH_READ_MS, in loop(), never in an interrupt.
void readTouch(unsigned long now) {
  if (now - lastTouchReadMs < TOUCH_READ_MS) return;
  lastTouchReadMs = now;

  uint16_t ditRaw = touchRead(DIT_PAD_PIN);
  uint16_t dahRaw = touchRead(DAH_PAD_PIN);
  ditPad.update(ditRaw, now);
  dahPad.update(dahRaw, now);

  if (touchMonitor && now - lastTouchPrintMs >= TOUCH_MONITOR_MS) {
    lastTouchPrintMs = now;
    char line[96];
    snprintf(line, sizeof(line), "touch GPIO %d left %u %s, GPIO %d right %u %s", DIT_PAD_PIN,
             ditRaw, ditPad.touched() ? "TOUCHED" : "free", DAH_PAD_PIN, dahRaw,
             dahPad.touched() ? "TOUCHED" : "free");
    printLine(line);
  }
}

// Give the keyer the paddle states on every pass of loop(), so each dit,
// dah and gap ends within a ms or so of its exact time. The keyer reports
// the exact time, so the decoder and the relay get perfect timing even if
// this pass came a few ms late.
void runKeyer(unsigned long now) {
  bool left = ditPad.touched();
  bool right = dahPad.touched();
  bool dit = paddlesSwapped ? right : left;
  bool dah = paddlesSwapped ? left : right;
  while (keyer.update(dit, dah, now)) updateKey(keyer.changeMs());
}

// Read the knob every KNOB_READ_MS. It also prints the raw reading whenever
// the WPM changes, as a wiring check. Near 0 or 4095 all the time means a
// leg is not connected, and numbers jumping all over mean the middle leg is
// loose.
void readKnob(unsigned long now) {
  if (now - lastKnobReadMs < KNOB_READ_MS) return;
  lastKnobReadMs = now;

  uint16_t raw = analogRead(SPEED_KNOB_PIN);
  if (KNOB_REVERSED) raw = SpeedKnob::ADC_MAX - raw;
  if (!knob.update(raw)) return;

  keyer.setWpm(knob.wpm());
  char line[64];
  snprintf(line, sizeof(line), "Speed knob raw %u, average %d, %d WPM", raw, knob.average(),
           knob.wpm());
  printLine(line);
  screenChanged = true;
}

void printSettings() {
  char line[80];
  snprintf(line, sizeof(line), "Keyer %d WPM, mode %c, dit pad GPIO %d, dah pad GPIO %d",
           knob.wpm(), keyer.mode() == KEYER_MODE_A ? 'A' : 'B',
           paddlesSwapped ? DAH_PAD_PIN : DIT_PAD_PIN, paddlesSwapped ? DIT_PAD_PIN : DAH_PAD_PIN);
  printLine(line);
  snprintf(line, sizeof(line), "Mic decoding %s, tone %s", micOn ? "on" : "off",
           micAutoMode() ? "auto (400 to 900 Hz)" : "fixed 600 Hz");
  printLine(line);
}

// The full list, at start up and for ?, so every command is always shown.
void printHelp() {
  printLine("Commands (type one, then Enter):");
  printLine("  a     keyer mode A");
  printLine("  b     keyer mode B");
  printLine("  swap  swap the dit and dah pads");
  printLine("  t     touch monitor on or off");
  printLine("  m     mic decoding on or off");
  printLine("  auto  mic tone auto (400 to 900 Hz) or fixed 600 Hz");
  printLine("  l     mic levels on or off (mic must be on)");
  printLine("  ?     settings and this list");
}

void printMicTone() {
  if (micAutoMode()) {
    printLine("Mic tone: auto, the strongest from 400 to 900 Hz");
  } else {
    printLine("Mic tone: fixed 600 Hz");
  }
}

void setMicOn(bool on) {
  micOn = on;
  if (on) {
    micDecoder.reset();
    micText.newLine();
    lastMicBlock = {};
    lastMicBlock.hz = 600;  // the detector always starts on 600 Hz
    micStart();
    printLine("Mic decoding on. Type l for levels, auto for auto tone, m to stop.");
    printMicTone();
  } else {
    micStop();
    micLevels = false;
    printLine("Mic decoding off");
  }
  screenChanged = true;
}

void runCommand(const char* text) {
  if (strcmp(text, "m") == 0) {
    setMicOn(!micOn);
    return;
  } else if (strcmp(text, "auto") == 0) {
    micSetAutoMode(!micAutoMode());
    printMicTone();
    screenChanged = true;
    return;
  } else if (strcmp(text, "l") == 0) {
    if (!micOn) {
      printLine("The mic is off. Type m first.");
      return;
    }
    micLevels = !micLevels;
    if (!micLevels) printLine("Mic levels off");
    lastLevelsMs = millis() - MIC_LEVELS_MS;  // first line straight away
    return;
  } else if (strcmp(text, "a") == 0) {
    keyer.setMode(KEYER_MODE_A);
  } else if (strcmp(text, "b") == 0) {
    keyer.setMode(KEYER_MODE_B);
  } else if (strcmp(text, "swap") == 0) {
    paddlesSwapped = !paddlesSwapped;
  } else if (strcmp(text, "t") == 0) {
    touchMonitor = !touchMonitor;
    if (touchMonitor) {
      // The limits first, so each live reading can be compared with them.
      if (lineHasText) {
        Serial.println();
        lineHasText = false;
      }
      printPadLevels("left", DIT_PAD_PIN, ditPad);
      printPadLevels("right", DAH_PAD_PIN, dahPad);
      lastTouchPrintMs = millis() - TOUCH_MONITOR_MS;  // first line straight away
    } else {
      printLine("Touch monitor off");
    }
    return;
  } else if (strcmp(text, "?") == 0) {
    printSettings();
    printHelp();
    return;
  } else {
    printLine("Unknown command. Type ? for the list.");
    return;
  }
  printSettings();
  screenChanged = true;
}

// Collect typed letters until Enter. Serial.available() says how many
// letters are already waiting, so this never waits for more.
void readSerialCommands() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r' || c == '\n') {
      if (commandLength > 0) {
        command[commandLength] = '\0';
        runCommand(command);
        commandLength = 0;
      }
    } else if (commandLength < (int)sizeof(command) - 1) {
      command[commandLength++] = (char)tolower(c);
    }
  }
}

// Everything from the network task: status lines for serial, key events
// from others, and the status shown on the screen. None of it waits.
void handleNetwork(unsigned long now) {
  char line[64];
  while (networkLogLine(line, sizeof(line))) printLine(line);

  KeyEvent event;
  while (networkReceive(&event)) {
    PlayTime time = remoteKeys.receive(event, now);
    if (DEBUG_EVENTS) {
      snprintf(line, sizeof(line), "recv %s %s %lu, plays in %ld ms%s", event.from,
               event.down ? "down" : "up", (unsigned long)event.t,
               (long)(int32_t)(time.playAt - now), time.reset ? ", delay reset" : "");
      printLine(line);
    }
  }

  Received got;
  while (remoteKeys.update(now, &got)) showReceived(got);
  remoteToneOn = remoteKeys.toneOn(now);

  NetStatus status = networkStatus();
  if (status != shownStatus) {
    shownStatus = status;
    screenChanged = true;
  }
}

// One line of mic tuning numbers, like
// "mic 600Hz fixed: tone 85.2 noise 3.1 on>40.1 off<20.0 ON 15wpm | 8000/s, 38us, missed 0"
void printMicLevels() {
  const MicBlock& b = lastMicBlock;
  MicStats stats = micStats();
  char line[160];
  snprintf(line, sizeof(line),
           "mic %dHz %s: tone %.1f noise %.1f on>%.1f off<%.1f %s %dwpm | %lu/s, %luus, missed %lu",
           (int)b.hz, micAutoMode() ? "auto" : "fixed", b.strength, b.noise, b.onLevel,
           b.offLevel, b.toneOn ? "ON" : "off", (int)(micDecoder.wpm() + 0.5f),
           (unsigned long)stats.samples, (unsigned long)stats.longestUs,
           (unsigned long)stats.missed);
  printLine(line);
}

// Everything from the mic task, one 10 ms block at a time. The mic decoder
// works in the sample clock's ms, from the blocks, not millis().
void handleMic(unsigned long now) {
  // Never decode my own sidetone or the relay tone from the speaker.
  bool sound = toneOn || remoteToneOn;
  if (sound) lastSoundMs = now;
  micSetIgnored(sound || now - lastSoundMs < MIC_HANGOVER_MS);
  if (!micOn) return;

  MicBlock block;
  while (micReceive(&block)) {
    lastMicBlock = block;
    if (block.changed) reportMicDecoded(micDecoder.toneChanged(block.toneOn, block.changeMs));
    reportMicDecoded(micDecoder.update(block.ms));

    // A long pause ends the mic's line, like it does for the key.
    if (lineHasText && lineIsMic && micDecoder.quietMs(block.ms) >= NEW_LINE_MS) {
      Serial.println();
      lineHasText = false;
      micText.newLine();
      screenChanged = true;
    }
  }

  int wpm = (int)(micDecoder.wpm() + 0.5f);
  if (lastMicBlock.hz != shownMicHz || lastMicBlock.toneOn != shownMicTone || wpm != shownMicWpm) {
    shownMicHz = lastMicBlock.hz;
    shownMicTone = lastMicBlock.toneOn;
    shownMicWpm = wpm;
    screenChanged = true;
  }

  if (micLevels && now - lastLevelsMs >= MIC_LEVELS_MS) {
    lastLevelsMs = now;
    printMicLevels();
  }
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
  attachInterrupt(digitalPinToInterrupt(KEY_PIN), onKeyEdge, CHANGE);

  // begin() starts I2C on our pins, sets up the SH1106 and clears it. It
  // blocks for a moment, which is fine here in setup().
  oled.setI2CAddress(OLED_I2C_ADDRESS);
  oled.setBusClock(OLED_BUS_HZ);
  oled.begin();
  drawScreen();
  printScreenTiming();

  // WiFi starts joining in the background now, so the touch calibration
  // below measures the pad with the radio already running, as it will be
  // in use. WiFi can add noise to touch readings.
  networkBegin();
  calibrateTouch();

  keyer.setMode(KEYER_MODE_AT_START);
  readKnob(millis());  // prints the first knob reading and sets the speed
  printSettings();
  printHelp();

  // The DAC starts at 0 V, but silence is the middle level, 128. Jumping
  // straight there would thump the speaker once, so slide up over 0.5 s.
  for (int level = 0; level <= DAC_MID; level++) {
    dacWrite(SIDETONE_PIN, level);  // also switches the DAC on, setup only
    delay(4);
  }

  // Builds the wave table once, here, never inside the interrupt.
  sidetone.begin(SIDETONE_HZ, SAMPLE_RATE_HZ, RAMP_MS, TONE_PRESET);
  remoteTone.begin(REMOTE_TONE_HZ, SAMPLE_RATE_HZ, RAMP_MS, TONE_PRESET);
  sampleTimer = timerBegin(SAMPLE_TIMER, TIMER_DIVIDER, true);
  timerAttachInterrupt(sampleTimer, &onSampleTimer, false);  // false: level, the only kind this core supports
  timerAlarmWrite(sampleTimer, TIMER_TICKS, true);  // true: repeat forever
  timerAlarmEnable(sampleTimer);

  // Sets up the mic sampling on timer 1, stopped until the m command.
  micBegin();
}

// No delay() here, so loop() runs thousands of times a second and never
// misses a press.
void loop() {
  // Read the edge time, then the pin, then the clock, and check the edge time
  // again. If an edge sneaks in between, this pass is skipped, so a new pin
  // reading is never paired with an old edge time.
  unsigned long edgeMs = lastEdgeMs;
  bool pressed = (digitalRead(KEY_PIN) == LOW);  // LOW means pressed, because of the pull-up
  unsigned long now = millis();
  bool settled = (lastEdgeMs == edgeMs) && (now - edgeMs >= DEBOUNCE_MS);

  // Quiet for DEBOUNCE_MS since the last edge, so this reading is real.
  // Times come from the last edge, when the contact stopped bouncing, not
  // from now. Both ends are measured the same way, so the times are true
  // even if loop() was busy with the screen.
  if (settled && pressed != buttonDown) {
    buttonDown = pressed;
    updateKey(edgeMs);
  }

  readTouch(now);
  runKeyer(now);
  readKnob(now);
  readSerialCommands();

  // While the key is up, tell the decoder how long the gap is so far, so a
  // letter prints as soon as the gap reaches its limit, without waiting for
  // the next press. Only when the button is steady, because during a bounce
  // the gap may already be over. The keyer needs no such check: it starts a
  // press at the moment it sees the paddle, never earlier.
  if (settled && !keyDown) {
    reportDecoded(decoder.keyUp(now - keyUpStartMs));

    // A long pause ends the line, but never prints empty lines.
    if (lineHasText && !lineIsMic && now - keyUpStartMs >= NEW_LINE_MS) {
      Serial.println();
      lineHasText = false;
      myText.newLine();
      screenChanged = true;
    }
  }

  handleNetwork(now);
  handleMic(now);
  updateScreen();
}
