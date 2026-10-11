#include "relay_host.h"

#include <string.h>

static const char* LOCAL_SUFFIX = ".local";

bool looksLikeIp(const char* host) {
  int parts = 0;
  const char* p = host;
  while (true) {
    // One number: 1 to 3 digits, no bigger than 255.
    int value = 0;
    int digits = 0;
    while (*p >= '0' && *p <= '9') {
      value = value * 10 + (*p - '0');
      digits++;
      p++;
      if (digits > 3) return false;
    }
    if (digits == 0 || value > 255) return false;
    parts++;

    if (*p == '\0') return parts == 4;
    if (*p != '.' || parts == 4) return false;
    p++;  // step over the dot
  }
}

static bool endsWithLocal(const char* host) {
  size_t length = strlen(host);
  size_t suffix = strlen(LOCAL_SUFFIX);
  return length > suffix && strcmp(host + length - suffix, LOCAL_SUFFIX) == 0;
}

RelayHostKind relayHostKind(const char* host) {
  if (host == nullptr || host[0] == '\0') return HOST_EMPTY;
  if (looksLikeIp(host)) return HOST_IP;
  if (endsWithLocal(host)) return HOST_MDNS;
  return HOST_NAME;
}

bool mdnsName(const char* host, char* out, size_t outSize) {
  if (!endsWithLocal(host)) return false;
  size_t length = strlen(host) - strlen(LOCAL_SUFFIX);
  if (length + 1 > outSize) return false;
  memcpy(out, host, length);
  out[length] = '\0';
  return true;
}
