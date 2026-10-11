# Setup Notes: the Mac as Relay and Web Server

How to run Phase 3 (internet Morse) on the home WiFi, and the Mac settings it
needs.

## macOS firewall
The phone and the ESP32 connect to the Mac over WiFi. If the macOS firewall
blocks Python, they cannot reach the web page (port 8000) or the relay
(port 8765), even though everything works from the Mac itself.

Fix, tested 2026-10-07:
1. System Settings, Network, Firewall, Options.
2. Add this app and set it to "Allow incoming connections":
   `/Library/Frameworks/Python.framework/Versions/3.13/Resources/Python.app`
3. In the file picker, press Cmd+Shift+G and paste the path above.

Why this path: python3.13 and the venv's python are only links. The program
that really runs is Python.app inside the python.org install, and that is
what the firewall checks. Updating Python to a new 3.13.x release may need
this to be done again.

## The Mac's IP address can change
The home router hands out addresses, and the Mac may get a different one
after a restart or a new day. Never write the IP into code or bookmarks.
Read it each time from the relay's first line at start up:

    Relay ready. Connect to ws://192.168.1.23:8765

The web page finds the relay by itself, using the address it was loaded
from, so only the phone's address bar needs the current IP.

## Running internet Morse
From the project folder, with the venv active, in two terminals:

    python -m relay.server
    python3.13 -m http.server 8000 --bind 0.0.0.0 --directory web

Then open:
- on the Mac: http://localhost:8000
- on the phone: http://<IP from the relay>:8000

Tap Start on each device. On an iPhone, switch off the silent switch, or the
page makes no sound.

## The ESP32 on the relay
The ESP32 needs the WiFi name and password and the Mac's name. They live in
`firmware/include/secrets.h`, which git ignores, so the password never
reaches the public repo. Only `secrets.example.h`, with blank values, is
committed.

1. If `secrets.h` is missing, copy `secrets.example.h` to `secrets.h` in
   the same folder.
2. Fill in `WIFI_SSID` and `WIFI_PASSWORD`. The ESP32 only joins 2.4 GHz
   WiFi, not 5 GHz.
3. Set `RELAY_HOST` in one of two forms:
   - A plain IP like `"192.168.0.188"`, as the relay prints it at start up.
     It is used directly, with no lookup. If the router gives the Mac a new
     IP, edit it and upload again.
   - The Mac's Bonjour name with `.local` on the end:
     `"Akritis-MacBook-Pro.local"`. It is looked up each time, so it keeps
     working when the IP changes. It is shown in System Settings, General,
     Sharing, Local hostname, or with `scutil --get LocalHostName`.
   - With the IP form, `RELAY_HOST` must match the IP the relay prints at
     start up (`Relay ready. Connect to ws://...`). On 2026-10-11 the Mac's
     IP changed three times in one day (.188, .169, .160). When the OLED
     stays on "WiFi only", compare the two first. The `.local` form avoids
     this.
4. Before committing, check that git cannot see the secrets:
   `git check-ignore -v firmware/include/secrets.h` must print the
   `.gitignore` line, and `git status` must not list `secrets.h`.

The ESP32 prints one serial line for each step: WiFi connected (with its own
IP), the relay's IP it found, relay connected, and relay disconnected.
