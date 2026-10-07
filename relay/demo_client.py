"""A tiny client for trying the relay by hand.

It connects, keys the letter K (-.-) as down and up events with real 15 WPM
timing, then keeps listening and prints every event it receives, until you
press Ctrl+C. Run two copies in two terminals to see events go both ways.

Run it from the project folder, with the venv active and the relay running:
    python -m relay.demo_client
    python -m relay.demo_client --url ws://192.168.1.23:8765 --name phone
"""

import argparse
import asyncio
import json
import time

from websockets.asyncio.client import connect

from morse_core.timing import text_to_key_events

TEXT = "K"
WPM = 15


def now_ms():
    """This program's own clock in ms, like millis() on the ESP32. Only the
    differences between two times mean anything, not the number itself."""
    return int(time.monotonic() * 1000)


async def send_event(websocket, kind, name):
    await websocket.send(json.dumps({"type": kind, "t": now_ms(), "from": name}))
    print(f"sent     {kind}", flush=True)


async def key_text(websocket, name):
    """Send each press as "down", wait its length, and each gap as "up"."""
    for key_down, duration_ms in text_to_key_events(TEXT, WPM):
        await send_event(websocket, "down" if key_down else "up", name)
        await asyncio.sleep(duration_ms / 1000)
    await send_event(websocket, "up", name)  # the list ends on a press, so let go


async def listen(websocket):
    async for message in websocket:
        print(f"received {message}", flush=True)


async def run(url, name):
    async with connect(url) as websocket:
        print(f"Connected to {url} as {name}. Keying {TEXT} at {WPM} WPM.", flush=True)
        await key_text(websocket, name)
        print("Done keying. Listening, press Ctrl+C to stop.", flush=True)
        await listen(websocket)


def main():
    parser = argparse.ArgumentParser(description="Send fake key events to the relay.")
    parser.add_argument("--url", default="ws://localhost:8765", help="relay address")
    parser.add_argument("--name", default="demo", help='the "from" name in each event')
    args = parser.parse_args()
    try:
        asyncio.run(run(args.url, args.name))
    except KeyboardInterrupt:
        print("Stopped.")


if __name__ == "__main__":
    main()
