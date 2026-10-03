// Phase 2, Session 1, Step 9: sidetone on a passive buzzer.
// While the button on GPIO 27 is held, a 600 Hz tone plays on GPIO 25 and both
// LEDs are on. On each release the time it was held is printed over serial.

#include <Arduino.h>
#include <morse.h>

// Pins, from the pin map in CLAUDE.md and docs/hardware-notes.md
const int ONBOARD_LED_PIN = 2;           // the blue LED built into the DevKit V1
const int LED_PIN = 26;                  // external LED on the breadboard
const int KEY_PIN = 27;                  // button to GND, INPUT_PULLUP
const int SIDETONE_PIN = 25;             // 330 ohm resistor, then passive buzzer to GND

// Sidetone, made by the LEDC (PWM) hardware so the CPU is free.
const int SIDETONE_HZ = 600;             // default pitch from the timing spec
const int SIDETONE_CHANNEL = 0;          // one of the 16 LEDC channels
const int SIDETONE_BITS = 8;             // duty goes from 0 to 255
const int SIDETONE_DUTY_ON = 128;        // half the time high, half low: a square wave

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

// Duty 0 holds the pin LOW all the time. No pulses means no sound and no hum,
// and no current flows through the buzzer while the key is up.
void setSidetone(bool on) {
  ledcWrite(SIDETONE_CHANNEL, on ? SIDETONE_DUTY_ON : 0);
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

  // Set the timer to 600 Hz once, link it to the pin, and start silent.
  ledcSetup(SIDETONE_CHANNEL, SIDETONE_HZ, SIDETONE_BITS);
  ledcAttachPin(SIDETONE_PIN, SIDETONE_CHANNEL);
  setSidetone(false);
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
