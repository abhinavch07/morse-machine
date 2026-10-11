#include "playout.h"

#include <string.h>

PlayTime planPlay(bool haveOffset, uint32_t* offset, uint32_t senderT, uint32_t nowMs,
                  uint32_t delayMs, uint32_t maxEarlyMs) {
  bool reset = !haveOffset;
  if (haveOffset) {
    int32_t wait = (int32_t)(senderT + *offset - nowMs);  // ms until its slot
    reset = wait < 0 || wait > (int32_t)(delayMs + maxEarlyMs);
  }
  if (reset) *offset = nowMs + delayMs - senderT;  // wraps safely, like millis()
  return {senderT + *offset, reset};
}

RemoteKeys::RemoteKeys(float ditDahSplitMs, float letterGapMs, float wordGapMs)
    : ditDahSplitMs_(ditDahSplitMs), letterGapMs_(letterGapMs), wordGapMs_(wordGapMs) {}

int RemoteKeys::findSender(const char* name, uint32_t nowMs) {
  for (int i = 0; i < MAX_SENDERS; i++) {
    if (senders_[i].used && strcmp(senders_[i].name, name) == 0) return i;
  }

  // New sender: take a free slot, or else the one not heard from longest.
  int pick = 0;
  for (int i = 0; i < MAX_SENDERS; i++) {
    if (!senders_[i].used) {
      pick = i;
      break;
    }
    if (nowMs - senders_[i].lastHeardMs > nowMs - senders_[pick].lastHeardMs) pick = i;
  }
  dropPlanned(pick);  // its old events must not play as the new sender's
  senders_[pick] = Sender();
  senders_[pick].used = true;
  strncpy(senders_[pick].name, name, MAX_SENDER_NAME);
  senders_[pick].decoder = KeyDecoder(ditDahSplitMs_, letterGapMs_, wordGapMs_);
  return pick;
}

void RemoteKeys::dropPlanned(int sender) {
  int kept = 0;
  for (int i = 0; i < queued_; i++) {
    if (queue_[i].sender != sender) queue_[kept++] = queue_[i];
  }
  queued_ = kept;
}

PlayTime RemoteKeys::receive(const KeyEvent& event, uint32_t nowMs) {
  int index = findSender(event.from, nowMs);
  Sender& s = senders_[index];
  s.lastHeardMs = nowMs;

  PlayTime time = planPlay(s.haveOffset, &s.offset, event.t, nowMs);
  s.haveOffset = true;
  if (queued_ == QUEUE_SIZE) return time;  // no room. Cannot happen at human speed.

  // Insert in time order, after any event with the same time, so events from
  // one sender always play in the order they were sent.
  int at = queued_;
  while (at > 0 && (int32_t)(queue_[at - 1].playAt - time.playAt) > 0) {
    queue_[at] = queue_[at - 1];
    at--;
  }
  queue_[at] = {index, event.down, event.t, time.playAt};
  queued_++;
  s.pending++;
  return time;
}

// Plays one event. Returns true if it finished a letter or word.
bool RemoteKeys::play(const Planned& event, uint32_t nowMs, Received* out) {
  Sender& s = senders_[event.sender];
  s.pending--;
  if (event.down == s.down) return false;  // a repeat, or an up with no down before it

  DecodeResult result = {false, false, nullptr, nullptr};
  if (event.down) {
    if (s.haveLastT) result = s.decoder.keyUp(event.t - s.lastT);  // the gap just ended
  } else {
    s.decoder.keyDown(event.t - s.lastT);
  }
  s.down = event.down;
  s.haveLastT = true;
  s.lastT = event.t;
  s.playedAtMs = nowMs;

  *out = {s.name, result};
  return result.letterDone || result.wordDone;
}

bool RemoteKeys::update(uint32_t nowMs, Received* out) {
  // Events that are due, soonest first.
  while (queued_ > 0 && timeReached(nowMs, queue_[0].playAt)) {
    Planned event = queue_[0];
    queued_--;
    memmove(&queue_[0], &queue_[1], queued_ * sizeof(Planned));
    if (play(event, nowMs, out)) return true;
  }

  // Gaps still growing, so a letter shows without waiting for the next press.
  // Skip a sender whose next event is already planned: it will give the exact gap.
  for (int i = 0; i < MAX_SENDERS; i++) {
    Sender& s = senders_[i];
    if (!s.used || s.down || !s.haveLastT || s.pending > 0) continue;
    DecodeResult result = s.decoder.keyUp(nowMs - s.playedAtMs);
    if (result.letterDone || result.wordDone) {
      *out = {s.name, result};
      return true;
    }
  }
  return false;
}

bool RemoteKeys::toneOn(uint32_t nowMs) const {
  for (const Sender& s : senders_) {
    if (s.used && s.down && nowMs - s.playedAtMs < STUCK_KEY_MS) return true;
  }
  return false;
}
