// The key event messages sent to and from the relay, as small JSON texts:
//   {"type":"down","t":12345,"from":"esp32"}
// The same format as web/index.html and relay/demo_client.py. It has no
// Arduino code in it, so the unit tests can run on the Mac too.

#pragma once

#include <stddef.h>
#include <stdint.h>

const int MAX_SENDER_NAME = 16;  // longer names are cut, like the web page does

struct KeyEvent {
  bool down;                        // true for "down", false for "up"
  uint32_t t;                       // the sender's own clock, in ms
  char from[MAX_SENDER_NAME + 1];   // the sender's name, plus the end marker
};

// Writes the event as JSON into out. Returns the length written, or 0 if out
// is too small.
size_t formatKeyEvent(const KeyEvent& event, char* out, size_t outSize);

// Reads one message from the relay. Returns false, and leaves *event alone,
// if it is not a key event: bad JSON, a type other than "down" or "up", or a
// t that is missing or not a whole number. A missing "from" becomes "?".
bool parseKeyEvent(const char* text, size_t length, KeyEvent* event);
