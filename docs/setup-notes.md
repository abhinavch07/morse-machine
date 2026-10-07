# Setup Notes: the Mac as Relay and Web Server

How to run Phase 3 (internet Morse) on the home WiFi, and the Mac settings it
needs.

## macOS firewall
The phone and the ESP32 connect to the Mac over WiFi. If the macOS firewall
blocks Python, they cannot reach the web page (port 8000) or the relay
(port 8765), even though everything works from the Mac itself.

Fix, tested 2026-10-08:
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
