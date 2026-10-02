// Phase 2, Session 1, Step 5: blink Morse on the onboard LED.
// Sends "VU" at 5 WPM on the blue LED, waits 2 seconds, then repeats.

#include <Arduino.h>
#include <morse.h>

const int LED_PIN = 2;                   // the onboard blue LED on the DevKit V1
const float WPM = 5;                     // slow, so the eye can follow it
const char* MESSAGE = "VU";
const unsigned long REPEAT_PAUSE_MS = 2000;

// Turn the LED on or off and hold it for a time. delay() only takes whole
// milliseconds, so the float time is rounded.
void keyLed(bool on, float ms) {
  digitalWrite(LED_PIN, on ? HIGH : LOW);
  delay(lroundf(ms));
}

// Blink one Morse code such as "...-". No gap after the last symbol, because
// the gap that follows depends on what comes next.
void sendCode(const char* code, const MorseTiming& t) {
  for (int i = 0; code[i] != '\0'; i++) {
    if (i > 0) keyLed(false, t.intraGap);
    keyLed(true, code[i] == '.' ? t.dit : t.dah);
  }
  digitalWrite(LED_PIN, LOW);
}

// Send a whole text. Same gap rules as totalDurationMs() in the library.
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

  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  sendText(MESSAGE, computeTiming(WPM));
  delay(REPEAT_PAUSE_MS);
}
