// Plays and decodes key events received from other devices, with the same
// rules as web/morse.js and web/index.html:
//   - each event plays PLAYOUT_DELAY_MS after it was sent, per sender, so an
//     uneven network does not make the rhythm uneven
//   - an event too late for its slot, or planned impossibly far ahead,
//     resets that sender's delay
//   - a key held down over STUCK_KEY_MS stops the tone
//   - letters are decoded from the sender's own t values, so network delays
//     cannot change the decoded text
// It has no Arduino code in it, so the unit tests can run on the Mac too.
// All times are millis() style: uint32_t ms that wrap after 49 days. Times
// are only ever compared through differences, which stay right across a wrap.

#pragma once

#include <stdint.h>

#include <key_decoder.h>
#include <key_event.h>

const uint32_t PLAYOUT_DELAY_MS = 200;
const uint32_t MAX_EARLY_MS = 1000;
const uint32_t STUCK_KEY_MS = 2000;

// True once nowMs has reached atMs.
inline bool timeReached(uint32_t nowMs, uint32_t atMs) {
  return (int32_t)(nowMs - atMs) >= 0;
}

struct PlayTime {
  uint32_t playAt;  // when to play the event, on our clock
  bool reset;       // true if the sender's offset was set again
};

// When to play one event. *offset turns the sender's t into our time, and
// haveOffset says whether it is set yet. The same rule as Playout.schedule()
// in web/morse.js: set the offset on the first event, and again when an event
// is too late or too far ahead, so that event plays delayMs from now.
PlayTime planPlay(bool haveOffset, uint32_t* offset, uint32_t senderT, uint32_t nowMs,
                  uint32_t delayMs = PLAYOUT_DELAY_MS, uint32_t maxEarlyMs = MAX_EARLY_MS);

// One letter or word gap decoded from a remote sender.
struct Received {
  const char* sender;   // the sender's name. Use it before the next receive().
  DecodeResult result;  // same as from the local decoder
};

class RemoteKeys {
 public:
  static const int MAX_SENDERS = 4;   // a fifth sender replaces the quietest one
  static const int QUEUE_SIZE = 32;   // events waiting to play, far more than 200 ms needs

  // The decoder limits, the same as for my own keying.
  RemoteKeys(float ditDahSplitMs, float letterGapMs, float wordGapMs);

  // A key event arrived from the relay. Plans when it plays.
  PlayTime receive(const KeyEvent& event, uint32_t nowMs);

  // Plays every event that is due, and checks gaps that are still growing.
  // Decoded letters and word gaps come out one at a time: keep calling while
  // it returns true.
  bool update(uint32_t nowMs, Received* out);

  // True while any sender's key is down, except a key down over STUCK_KEY_MS.
  bool toneOn(uint32_t nowMs) const;

 private:
  struct Sender {
    bool used = false;
    char name[MAX_SENDER_NAME + 1] = "";
    bool haveOffset = false;
    uint32_t offset = 0;
    uint32_t lastHeardMs = 0;   // our time of the last message, to pick who to replace
    int pending = 0;            // events planned but not played yet
    // Set when events play:
    bool down = false;
    bool haveLastT = false;
    uint32_t lastT = 0;         // sender's t of the last event played
    uint32_t playedAtMs = 0;    // our time when the last event played
    KeyDecoder decoder = KeyDecoder(0, 0, 0);  // real limits set in findSender()
  };

  struct Planned {
    int sender;  // index into senders_
    bool down;
    uint32_t t;
    uint32_t playAt;
  };

  int findSender(const char* name, uint32_t nowMs);
  void dropPlanned(int sender);
  bool play(const Planned& event, uint32_t nowMs, Received* out);

  float ditDahSplitMs_;
  float letterGapMs_;
  float wordGapMs_;
  Sender senders_[MAX_SENDERS];
  Planned queue_[QUEUE_SIZE];  // sorted by playAt, soonest first
  int queued_ = 0;
};
