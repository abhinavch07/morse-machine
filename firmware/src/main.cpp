// Phase 2, Session 1, Step 8: push button as a key.
// While the button on GPIO 27 is held, both LEDs are on. On each release the
// time it was held is printed over serial.

#include <Arduino.h>
#include <morse.h>

// Pins, from the pin map in CLAUDE.md and docs/hardware-notes.md
const int ONBOARD_LED_PIN = 2;           // the blue LED built into the DevKit V1
const int LED_PIN = 26;                  // external LED on the breadboard
const int KEY_PIN = 27;                  // button to GND, INPUT_PULLUP

// A reading must stay the same this long before we believe it.
const unsigned long DEBOUNCE_MS = 10;

// Kept for sendText(), which is not used right now but will be later.
const float WPM = 5;
const char* MESSAGE = "VU";

// Key state, kept between passes of loop().
int lastReading = HIGH;                  // raw pin value from the last pass
unsigned long lastChangeMs = 0;          // when the raw value last changed
bool keyDown = false;                    // the cleaned up, debounced state
unsigned long keyDownStartMs = 0;        // when the current press began

// Both LEDs always switch together, so they show the same Morse.
void setLeds(bool on) {
  digitalWrite(ONBOARD_LED_PIN, on ? HIGH : LOW);
  digitalWrite(LED_PIN, on ? HIGH : LOW);
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
      if (keyDown) {
        keyDownStartMs = now;
      } else {
        // Both ends are seen 10 ms late, so the delays cancel out.
        Serial.print("key down ");
        Serial.print(now - keyDownStartMs);
        Serial.println(" ms");
      }
    }
  }
}
