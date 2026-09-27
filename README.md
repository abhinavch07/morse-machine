# Morse Machine

A learning project to master Morse code (CW) and build a Morse machine from
scratch. The plan covers trainer software, a DIY key and paddle, an ESP32
keyer, internet Morse, a tone and ML decoder, and later a small QRP CW
transmitter after licensing.

This is a personal learn by doing project. It is built in phases, one step at a
time. See CLAUDE.md for the full phase plan and the Morse timing spec.

## Requirements

- Python 3.13 (run it as `python3.13`)
- git

## Set up the development environment

Create and activate a virtual environment, then install the dev tools.

```bash
python3.13 -m venv .venv
source .venv/bin/activate
pip install -r requirements-dev.txt
```

The virtual environment keeps this project's Python packages separate from the
system Python. Activate it every time you work on the project.

## Run the tests

With the virtual environment active:

```bash
pytest
```

## Repo layout

- `morse_core/` Python core: encode, decode, timing, audio
- `trainer/` Koch trainer and fist analyser
- `firmware/` PlatformIO project for the ESP32
- `relay/` WebSocket relay server for internet Morse
- `web/` browser key, sounder and decoder
- `decoder/` DSP tone decoder and ML decoder
- `tests/` tests for the core logic
- `docs/` progress log, learning notes and hardware notes

## Licence

MIT. See the LICENSE file.
