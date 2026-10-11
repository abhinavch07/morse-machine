// Template for secrets.h. This copy has no real values and is safe to commit.
//
// To use it: copy this file to secrets.h in the same folder, then fill in
// secrets.h. secrets.h is listed in .gitignore, so git never uploads it and
// the WiFi password never reaches the public repo.

#pragma once

// The home WiFi name and password. The ESP32 only works on 2.4 GHz WiFi.
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// The Mac running the relay, in one of two forms:
//
//   #define RELAY_HOST "192.168.0.188"
//     A plain IP. Used directly, with no lookup. Simple, but the router can
//     give the Mac a new IP, and then this must be changed and uploaded again.
//     The relay prints the Mac's current IP when it starts.
//
//   #define RELAY_HOST "Akritis-MacBook-Pro.local"
//     The Mac's Bonjour name. Looked up on the home network each time, so it
//     keeps working when the IP changes. Find the name in System Settings,
//     General, Sharing, Local hostname, or with: scutil --get LocalHostName
#define RELAY_HOST ""
#define RELAY_PORT 8765
