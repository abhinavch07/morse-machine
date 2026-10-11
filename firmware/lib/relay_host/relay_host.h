// Works out what kind of address RELAY_HOST in secrets.h is, so the network
// code knows how to find the relay. It has no Arduino code in it, so the
// unit tests can run on the Mac too.

#pragma once

#include <stddef.h>

enum RelayHostKind {
  HOST_EMPTY,  // nothing filled in
  HOST_IP,     // four numbers like "192.168.0.188": use it directly, no lookup
  HOST_MDNS,   // a Bonjour name like "Akritis-MacBook-Pro.local": look up with mDNS
  HOST_NAME,   // any other name: look up with normal DNS
};

// True only for four whole numbers from 0 to 255 with dots between them,
// like "192.168.0.188". No spaces, signs or other characters.
bool looksLikeIp(const char* host);

RelayHostKind relayHostKind(const char* host);

// For a HOST_MDNS name, writes the part before ".local" into out, because the
// mDNS lookup wants "Akritis-MacBook-Pro", not "Akritis-MacBook-Pro.local".
// Returns false if it does not fit or the host is not a .local name.
bool mdnsName(const char* host, char* out, size_t outSize);
