// WiFi and the relay connection, run in their own task on core 0.
//
// loop() runs on core 1 and must never wait for the network: a connect to a
// Mac that is switched off can take 5 seconds. So everything network lives in
// a separate FreeRTOS task, and loop() talks to it only through queues
// (mailboxes that are safe between tasks). Every function here returns at
// once, whatever the network is doing.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <key_event.h>

enum NetStatus : uint8_t {
  NET_NO_WIFI,    // not on WiFi (or no WiFi name in secrets.h)
  NET_WIFI,       // on WiFi, but not connected to the relay
  NET_RELAY,      // connected to the relay
};

// Starts WiFi and the network task. Call once in setup().
void networkBegin();

// Hands one of my key events to the network task to send. Returns false if
// the relay is not connected, and then the event is dropped: an old key
// event would be useless later.
bool networkSend(bool down, uint32_t t);

// Takes one event received from the relay, if there is one.
bool networkReceive(KeyEvent* event);

// Takes one status line for serial, like "relay connected", if there is one.
// The network task never prints itself, so its lines cannot cut into the
// middle of a line that loop() is printing.
bool networkLogLine(char* line, size_t size);

NetStatus networkStatus();
