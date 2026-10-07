"""WebSocket relay for internet Morse.

Every client (the ESP32, a browser, the demo client) connects here. Each key
event a client sends is passed on to all the other clients, never back to the
sender. The relay does not change messages and does not decode Morse. It only
checks that a message looks like a key event before passing it on.

Run it from the project folder, with the venv active:
    python -m relay.server
"""

import asyncio
import json
import socket
import time

from websockets.asyncio.server import broadcast, serve
from websockets.exceptions import ConnectionClosedError

# 0.0.0.0 means "every network the Mac is on", so devices on the home WiFi can
# connect, not only programs on the Mac itself.
HOST = "0.0.0.0"
PORT = 8765


def local_ip():
    """The Mac's address on the home network, like 192.168.1.23.

    Connecting a UDP socket sends nothing. It only makes the Mac choose which
    network it would use to reach the outside world, and that network's
    address is the one other devices on the WiFi can reach.
    """
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        try:
            s.connect(("8.8.8.8", 80))
            return s.getsockname()[0]
        except OSError:
            return "127.0.0.1"  # no network at all, only this Mac can connect


def parse_event(message):
    """The message as a dict if it is a key event, otherwise None.

    A key event is a JSON object with a "type" text field, for example
    {"type": "down", "t": 12345, "from": "esp32"}.
    """
    if not isinstance(message, str):
        return None  # binary frame, our clients only send text
    try:
        event = json.loads(message)
    except json.JSONDecodeError:
        return None
    if not isinstance(event, dict) or not isinstance(event.get("type"), str):
        return None
    return event


def log(action, client, detail=""):
    """One short line, like: 12:04:31 message    192.168.1.40:52144 esp32 down t=12345"""
    stamp = time.strftime("%H:%M:%S")
    print(f"{stamp} {action:<10} {client} {detail}".rstrip(), flush=True)


def client_name(websocket):
    """The client's address and port, like 192.168.1.40:52144."""
    address = websocket.remote_address
    return f"{address[0]}:{address[1]}" if address else "unknown"


class Relay:
    """Keeps the set of connected clients and passes messages between them."""

    def __init__(self):
        self.clients = set()

    async def handler(self, websocket):
        """Runs once for each client, for as long as it stays connected."""
        name = client_name(websocket)
        self.clients.add(websocket)
        log("connect", name, f"({len(self.clients)} online)")
        how = ""
        try:
            async for message in websocket:
                event = parse_event(message)
                if event is None:
                    log("drop", name, f"not a key event: {str(message)[:40]}")
                    continue
                others = self.clients - {websocket}
                # broadcast() sends to each one without waiting, so one slow
                # client cannot hold up the others.
                broadcast(others, message)
                detail = f"{event.get('from', '?')} {event['type']} t={event.get('t', '?')}"
                log("message", name, f"{detail} -> {len(others)} other(s)")
        except ConnectionClosedError:
            # The client left without a proper goodbye: WiFi lost, ESP32 reset,
            # or Ctrl+C. Normal for the relay, so one line, not a traceback.
            how = "suddenly, "
        finally:
            # Runs however the client left.
            self.clients.discard(websocket)
            log("disconnect", name, f"({how}{len(self.clients)} online)")


async def run(host=HOST, port=PORT):
    relay = Relay()
    async with serve(relay.handler, host, port) as server:
        print(f"Relay ready. Connect to ws://{local_ip()}:{port}", flush=True)
        print("Press Ctrl+C to stop.", flush=True)
        await server.serve_forever()


def main():
    try:
        asyncio.run(run())
    except KeyboardInterrupt:
        print("Relay stopped.")


if __name__ == "__main__":
    main()
