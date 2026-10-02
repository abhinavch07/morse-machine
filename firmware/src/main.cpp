// Phase 2, Session 1: first ESP32 program.
// Prints a hello message, then blinks the onboard blue LED forever.

#include <Arduino.h>

const int LED_PIN = 2;          // the onboard blue LED on the DevKit V1
const int BLINK_MS = 500;       // time on, and time off

// setup() runs once, each time the board powers up or is reset.
void setup() {
  Serial.begin(115200);
  delay(1000);  // give the Mac a moment to open the serial monitor
  Serial.println("Hello from ESP32, Morse machine starting");

  pinMode(LED_PIN, OUTPUT);
}

// loop() runs again and again, forever, after setup() finishes.
void loop() {
  digitalWrite(LED_PIN, HIGH);
  Serial.println("LED on");
  delay(BLINK_MS);

  digitalWrite(LED_PIN, LOW);
  Serial.println("LED off");
  delay(BLINK_MS);
}
