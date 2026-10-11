#include "key_event.h"

#include <ArduinoJson.h>
#include <string.h>

size_t formatKeyEvent(const KeyEvent& event, char* out, size_t outSize) {
  JsonDocument doc;
  doc["type"] = event.down ? "down" : "up";
  doc["t"] = event.t;
  doc["from"] = event.from;
  // measureJson() gives the length without the end marker, so the text only
  // fits if there is room for one more char.
  if (measureJson(doc) + 1 > outSize) return 0;
  return serializeJson(doc, out, outSize);
}

bool parseKeyEvent(const char* text, size_t length, KeyEvent* event) {
  JsonDocument doc;
  if (deserializeJson(doc, text, length)) return false;  // not valid JSON

  const char* type = doc["type"];
  if (type == nullptr) return false;
  bool down;
  if (strcmp(type, "down") == 0) {
    down = true;
  } else if (strcmp(type, "up") == 0) {
    down = false;
  } else {
    return false;
  }

  // is<uint32_t>() is false for text, negative numbers and fractions.
  if (!doc["t"].is<uint32_t>()) return false;

  event->down = down;
  event->t = doc["t"];
  const char* from = doc["from"] | "?";
  strncpy(event->from, from, MAX_SENDER_NAME);
  event->from[MAX_SENDER_NAME] = '\0';  // strncpy does not add it when cutting
  return true;
}
