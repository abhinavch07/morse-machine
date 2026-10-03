// Phase 2, Session 2, Step 15: show decoded text on the OLED.
// While the button on GPIO 27 is held, a 600 Hz tone with a 5 ms fade in and
// fade out plays on GPIO 25 (DAC1), and both LEDs are on. The press and gap
// times go to the decoder. Letters print on one line as they are decoded, a
// word gap prints a space, and 5 seconds with no presses ends the line.
// The SH1106 OLED shows the same text, plus the dits and dahs of the letter
// being keyed now.

#include <Arduino.h>
#include <soc/rtc_io_reg.h>
#include <U8g2lib.h>
#include <key_decoder.h>
#include <morse.h>
#include <screen_text.h>
#include <sidetone.h>

// Pins, from the pin map in CLAUDE.md and docs/hardware-notes.md
const int ONBOARD_LED_PIN = 2;           // the blue LED built into the DevKit V1
const int LED_PIN = 26;                  // external LED on the breadboard
const int KEY_PIN = 27;                  // button to GND, INPUT_PULLUP
const int SIDETONE_PIN = 25;             // DAC1, to PAM8403 input L
const int OLED_SDA_PIN = 21;             // I2C data
const int OLED_SCL_PIN = 22;             // I2C clock

// The JMD1.3A board is set to address 0x78. That is the 8 bit form, which is
// what U8g2 wants. Most I2C scanners show the 7 bit form, 0x3C.
const uint8_t OLED_I2C_ADDRESS = 0x78;
const uint32_t OLED_BUS_HZ = 400000;     // 400 kHz "fast mode" I2C

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
bool keyDown = false;                    // the cleaned up, debounced state
unsigned long keyDownStartMs = 0;        // when the current press began
unsigned long keyUpStartMs = 0;          // when the current gap began

KeyDecoder decoder(DIT_DAH_SPLIT_MS, LETTER_GAP_MS, WORD_GAP_MS);
bool lineHasText = false;                // printed something since the last new line

// Set by the key pin interrupt at every raw change, bounces included. loop()
// uses this time, not the time it happens to notice the change, so a slow
// screen update cannot make a press or gap look longer or shorter.
volatile unsigned long lastEdgeMs = 0;

// The screen. F means the whole picture is kept in a 1 KB buffer in RAM, so
// we can draw it all at once and then send it to the screen in pieces.
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE, OLED_SCL_PIN, OLED_SDA_PIN);
ScreenText screenText;
bool screenChanged = true;               // draw once at start up
int nextPage = -1;                       // next 8 pixel row to send, -1 when idle

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
    Serial.print(r.letter);
    lineHasText = true;
  }
  if (r.wordDone) Serial.print(' ');
}

// Same as printDecoded(), but for the screen.
void showDecoded(const DecodeResult& r) {
  if (r.letterDone) screenText.addLetter(r.letter);
  if (r.wordDone) screenText.addSpace();
  if (r.letterDone || r.wordDone) screenChanged = true;
}

void reportDecoded(const DecodeResult& r) {
  printDecoded(r);
  showDecoded(r);
}

// Draw the whole picture into the RAM buffer. Nothing goes to the screen
// yet, so this is fast, well under a millisecond.
void drawScreen() {
  oled.clearBuffer();

  // Top: a small title and a line under it.
  oled.setFont(u8g2_font_5x7_tr);
  oled.drawStr(0, 7, "MORSE");
  oled.drawHLine(0, 9, 128);

  // Middle: four lines of decoded text, 10 pixels apart.
  oled.setFont(u8g2_font_6x10_tr);
  for (int row = 0; row < ScreenText::ROWS; row++) {
    oled.drawStr(0, 20 + row * 10, screenText.line(row));
  }

  // Bottom: a line, then the dits and dahs of the letter being keyed now.
  oled.drawHLine(0, 53, 128);
  char code[20];
  oled.drawStr(0, 63, spacedCode(decoder.currentCode(), code));
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
  attachInterrupt(digitalPinToInterrupt(KEY_PIN), onKeyEdge, CHANGE);

  // begin() starts I2C on our pins, sets up the SH1106 and clears it. It
  // blocks for a moment, which is fine here in setup().
  oled.setI2CAddress(OLED_I2C_ADDRESS);
  oled.setBusClock(OLED_BUS_HZ);
  oled.begin();
  drawScreen();
  printScreenTiming();

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
  // Read the edge time, then the pin, then the clock, and check the edge time
  // again. If an edge sneaks in between, this pass is skipped, so a new pin
  // reading is never paired with an old edge time.
  unsigned long edgeMs = lastEdgeMs;
  bool pressed = (digitalRead(KEY_PIN) == LOW);  // LOW means pressed, because of the pull-up
  unsigned long now = millis();
  bool settled = (lastEdgeMs == edgeMs) && (now - edgeMs >= DEBOUNCE_MS);

  // Quiet for DEBOUNCE_MS since the last edge, so this reading is real.
  if (settled && pressed != keyDown) {
    keyDown = pressed;
    setLeds(keyDown);
    setSidetone(keyDown);
    // Times come from the last edge, when the contact stopped bouncing, not
    // from now. Both ends are measured the same way, so the times are true
    // even if loop() was busy with the screen.
    if (keyDown) {
      reportDecoded(decoder.keyUp(edgeMs - keyUpStartMs));  // the gap just ended
      printTiming("up", edgeMs - keyUpStartMs);
      keyDownStartMs = edgeMs;
    } else {
      decoder.keyDown(edgeMs - keyDownStartMs);
      printTiming("down", edgeMs - keyDownStartMs);
      keyUpStartMs = edgeMs;
      screenChanged = true;  // one more dit or dah on the bottom line
    }
  }

  // While the key is up, tell the decoder how long the gap is so far, so a
  // letter prints as soon as the gap reaches LETTER_GAP_MS, without waiting for
  // the next press. Only when settled, because during a bounce the gap may
  // already be over.
  if (settled && !keyDown) {
    reportDecoded(decoder.keyUp(now - keyUpStartMs));

    // A long pause ends the line, but never prints empty lines.
    if (lineHasText && now - keyUpStartMs >= NEW_LINE_MS) {
      Serial.println();
      lineHasText = false;
      screenText.newLine();
      screenChanged = true;
    }
  }

  updateScreen();
}
