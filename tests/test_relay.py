"""Tests for the WebSocket relay.

Each test starts a real relay on the Mac, on a free port chosen by the system,
and connects real clients to it. The async steps run inside asyncio.run(), so
no extra test package is needed.
"""

import asyncio
import json

import pytest
from websockets.asyncio.client import connect
from websockets.asyncio.server import serve

from relay.server import Relay, parse_event

DOWN = json.dumps({"type": "down", "t": 12345, "from": "esp32"})

# How long to wait to be sure a message is NOT coming.
QUIET_S = 0.2


async def start_relay():
    """A running relay on 127.0.0.1, its Relay object and its ws:// address."""
    relay = Relay()
    server = await serve(relay.handler, "127.0.0.1", 0)  # port 0: pick a free one
    port = server.sockets[0].getsockname()[1]
    return server, relay, f"ws://127.0.0.1:{port}"


async def wait_for_clients(relay, count):
    """A client's connect() returns a moment before the relay has added it to
    its set. Wait for that, so no message is sent before everyone is in."""
    for _ in range(100):
        if len(relay.clients) == count:
            return
        await asyncio.sleep(0.01)
    raise AssertionError(f"expected {count} clients, relay has {len(relay.clients)}")


async def assert_nothing_arrives(client):
    with pytest.raises(TimeoutError):
        await asyncio.wait_for(client.recv(), QUIET_S)


def test_message_reaches_other_client_not_sender():
    async def scenario():
        server, relay, url = await start_relay()
        async with server, connect(url) as a, connect(url) as b:
            await wait_for_clients(relay, 2)
            await a.send(DOWN)
            assert await asyncio.wait_for(b.recv(), 1) == DOWN
            await assert_nothing_arrives(a)

    asyncio.run(scenario())


def test_message_reaches_every_other_client():
    async def scenario():
        server, relay, url = await start_relay()
        async with server, connect(url) as a, connect(url) as b, connect(url) as c:
            await wait_for_clients(relay, 3)
            await b.send(DOWN)
            assert await asyncio.wait_for(a.recv(), 1) == DOWN
            assert await asyncio.wait_for(c.recv(), 1) == DOWN
            await assert_nothing_arrives(b)

    asyncio.run(scenario())


def test_bad_messages_are_dropped():
    async def scenario():
        server, relay, url = await start_relay()
        async with server, connect(url) as a, connect(url) as b:
            await wait_for_clients(relay, 2)
            await a.send("hello")             # not JSON
            await a.send('{"t": 1}')          # JSON, but no "type"
            await a.send(b'{"type": "down"}')  # binary, not text
            await a.send(DOWN)
            # Messages arrive in order, so if the first one B gets is DOWN,
            # the three bad ones were not passed on.
            assert await asyncio.wait_for(b.recv(), 1) == DOWN

    asyncio.run(scenario())


def test_relay_forgets_a_client_that_leaves():
    async def scenario():
        server, relay, url = await start_relay()
        async with server, connect(url) as a:
            async with connect(url):
                await wait_for_clients(relay, 2)
            await wait_for_clients(relay, 1)
            await a.send(DOWN)  # nobody else is left, and this must not fail
            await assert_nothing_arrives(a)

    asyncio.run(scenario())


def test_sudden_disconnect_is_not_an_error(caplog):
    # The ESP32 losing WiFi looks like this: the connection just stops, with
    # no goodbye. websockets logs an ERROR if our handler lets that escape.
    async def scenario():
        server, relay, url = await start_relay()
        async with server, connect(url) as a:
            b = await connect(url)
            await wait_for_clients(relay, 2)
            b.transport.abort()  # cut the connection, no closing handshake
            await wait_for_clients(relay, 1)
            await a.send(DOWN)  # the relay still works for the client left
            await assert_nothing_arrives(a)

    asyncio.run(scenario())
    errors = [r for r in caplog.records if r.levelname == "ERROR"]
    assert errors == []


@pytest.mark.parametrize(
    "message",
    [
        "hello",
        "[1, 2]",
        '{"t": 1}',
        '{"type": 5}',
        b'{"type": "down"}',
    ],
)
def test_parse_event_rejects(message):
    assert parse_event(message) is None


def test_parse_event_accepts_key_event():
    assert parse_event(DOWN) == {"type": "down", "t": 12345, "from": "esp32"}
