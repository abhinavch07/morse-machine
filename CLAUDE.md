# Morse Machine

A learning project to master Morse code (CW) and build a Morse machine from scratch: trainer software, a DIY key and paddle, an ESP32 keyer, internet Morse, a decoder with an ML model, and later (after licensing) a small QRP CW transmitter.

The owner (Abhinav) is a beginner in electronics and Morse. Treat every task as a teaching moment. Explain what the code does and why, in plain simple English.

## Working rules for Claude Code
- Ask clarifying questions before assuming anything about hardware, pins, parts or goals.
- Work one phase at a time. Do not start a later phase unless asked.
- Explain each new file in 2 to 4 plain sentences after writing it.
- Keep code small and readable over clever. Comment the "why", not the obvious.
- Write tests for the Morse core logic (encoding, decoding, timing).
- After each session, add a dated entry to `docs/progress-log.md`: what was built, what was learned, what is next.
- No em dashes and no semicolons in docs, comments or messages.

## Safety and legal rules (hard rules)
- The owner has NO amateur radio licence yet. Do not write code or give steps that key a real radio transmitter or an antenna until the owner confirms a licence and states its grade in this file.
- Licence status: NOT LICENSED (update this line after the licence is issued)
- Any RF circuit work before licensing is tested into a dummy load only.
- No mains (230 V AC) circuits. USB, batteries or small DC adapters only.

## Dev machine
- Intel MacBook Pro (x86_64), macOS, zsh shell.
- Python 3.13 installed from python.org. Run it as `python3.13`. Plain `python3` may point to the old macOS Python 3.9, so do not use it.
- Always use a project virtual environment: `python3.13 -m venv .venv` then `source .venv/bin/activate`.
- Homebrew no longer supports Intel Macs. Do not suggest `brew install` for Python or other tools. Prefer pip inside the venv, official installers, or ask first.
- Do not suggest tools that need Apple Silicon.

## Tech stack
| Area | Choice | Why |
|---|---|---|
| Core Morse library, trainer, decoder | Python 3.13 | Easy to learn, great audio and ML libraries |
| Firmware | ESP32 with Arduino framework via PlatformIO | Cheap, has WiFi for internet Morse, good tooling |
| Internet relay | Python WebSocket server | Simple, runs on a laptop first |
| Browser client | Plain HTML + JavaScript (Web Audio API) | No build step, works on phone too |
| ML decoder | PyTorch, trained on Google Colab (free GPU). scikit-learn for small local tests. | Current PyTorch has no Intel Mac builds, and Colab is faster anyway |

## Repo layout
```
morse-machine/
  CLAUDE.md
  docs/
    progress-log.md
    learning-notes.md      # Morse and radio theory notes, ASOC topics marked
    hardware-notes.md      # circuits, pin maps, photos, parts bought
  morse_core/              # Python: encode, decode, timing, audio
  trainer/                 # Python: Koch trainer, fist analyser
  firmware/                # PlatformIO project for ESP32
  relay/                   # WebSocket relay server
  web/                     # browser key, sounder and decoder
  decoder/                 # DSP tone decoder and ML decoder
  tests/
```

## Morse timing spec (use everywhere)
- Standard word is "PARIS" (50 units).
- 1 unit (dit length) in ms = 1200 / WPM
- Dah = 3 units. Gap inside a character = 1 unit. Gap between characters = 3 units. Gap between words = 7 units.
- Farnsworth timing: characters sent at speed c WPM, spacing stretched so overall speed is s WPM (s is less than c).
  - Total extra delay per word, ta (seconds) = (60 x c minus 37.2 x s) / (s x c)
  - Gap between characters = 3 x ta / 19
  - Gap between words = 7 x ta / 19
- Default sidetone: 600 Hz, adjustable from 400 to 900 Hz. Add a 5 ms rise and fall ramp to avoid clicks.
- Support ITU letters, figures and the punctuation in the ASOC test: full stop, comma, semicolon, break sign, hyphen, question mark. Also prosigns AR, SK, BT, KN.

## Phases
Each phase ends with a clear "done when" check. Morse practice (Phase 0) runs every day across all phases.

### Phase 0: Learn Morse by ear (ongoing)
- Koch method with Farnsworth spacing. Characters at 20 WPM, spacing starting slow.
- 15 to 20 minutes daily. Learn sounds, never count dots and dashes or use visual charts.
- Done when: copy 8 WPM plain text with 90 percent accuracy for 5 minutes, and send it cleanly. This is the ASOC General grade level. Then push to 12 WPM for safety margin.

### Phase 1: Morse core and trainer (software only, Rs 0)
- `morse_core`: text to Morse, Morse to text, timing engine, WAV and live audio output.
- `trainer`: Koch trainer that tracks accuracy per character and repeats weak ones more.
- Done when: tests pass and the trainer plays a Koch lesson at chosen WPM and Farnsworth spacing.

### Phase 2: First hardware, straight key and sidetone
- DIY straight key (wood base, brass strip or hacksaw blade, screw contacts).
- ESP32 reads the key with debounce and plays sidetone on buzzer or small speaker, with LED.
- 1.3 inch OLED screen (SH1106 driver, I2C, 128x64) shows decoded text and WPM. Use the U8g2 library. Note: the driver is SH1106, not SSD1306.
- Learn: Ohm's law, pull-up resistors, contact bounce, I2C, using a multimeter, first soldering.
- Done when: pressing the key gives a clean tone, and the decoded characters show on the OLED and over serial.

### Phase 3: Internet Morse
- `relay`: WebSocket server that forwards key-down and key-up events with timestamps.
- `firmware`: ESP32 connects over WiFi and sends key events. Plays received events as sidetone.
- `web`: browser page to key with keyboard or touch, hear others, and see decoded text.
- Done when: two devices (ESP32 and browser, or two browsers) exchange Morse in real time.

### Phase 4: Iambic paddle and keyer
- Design and build a DIY dual lever paddle (microswitches or springy metal contacts).
- Firmware iambic keyer, mode A and mode B, with dot and dash memory. Speed set by a potentiometer.
- Done when: the paddle sends clean, well timed characters at 15 WPM.

### Phase 5: Receive side and tone decoder
- Microphone module on the ESP32 and a Goertzel filter to detect the tone and decode it.
- Listen to real HF CW through WebSDR or KiwiSDR in the browser and practise copying.
- Done when: the decoder reads clean audio at 15 WPM and adapts to the sender's speed.

### Phase 6: AI angle
- ML CW decoder: generate synthetic Morse audio with noise, fading, drift and uneven human timing. Train a small model (start with a spectrogram plus CNN or CRNN with CTC loss) in a Google Colab notebook kept in `decoder/`. Data generation and testing can run locally. Test on recordings saved from WebSDR.
- Adaptive trainer: pick practice characters and words from the owner's error history.
- Fist analyser: measure the owner's dit and dah ratios and gaps from key events and show how to improve.
- Done when: the ML decoder beats the Goertzel decoder on noisy test audio, with numbers logged in `docs/`.

### Phase 7: RF theory and ASOC prep (no transmitting)
- Study the ASOC syllabus: electronics basics, radio theory, antennas, rules and regulations, Q codes.
- Learn by simulation and bench work: oscillators, crystal oscillators, low pass filters, dummy load.
- Done when: the owner passes mock tests and clears the ASOC exam.

### Phase 8: QRP CW transmitter (only after licence)
- Only start when the licence status line above says LICENSED with grade.
- Design or assemble a small QRP CW transmitter for a band the licence allows. A low pass filter is mandatory. Test into a dummy load first, then on air.
- Confirm bands, CW sub-bands and power from the licence document before any on-air test.
- Done when: first on-air CW contact is logged.

## Starter parts list (target about Rs 3,000)
Prices are rough guesses. Check current prices before buying.
| Part | Approx Rs | Phase |
|---|---|---|
| ESP32 DevKit V1, 30 pin, CP2102 USB chip | 400 | 2 |
| 1.3 inch OLED, 128x64, I2C, 4 pin, SH1106 | 300 | 2 |
| Two breadboards (830 point) and jumper wires (male-male and male-female) | 300 | 2 |
| Resistor kit and 5 mm LEDs | 150 | 2 |
| Buzzers, 2 passive and 1 active | 50 | 2 |
| Small 8 ohm speaker and PAM8403 amplifier module | 120 | 2 |
| Microswitches and tactile switches | 80 | 2, 4 |
| Two 10k potentiometers (speed, volume) | 40 | 4 |
| 3.5 mm stereo jacks and cable | 80 | 2, 4 |
| Soldering iron kit (25 W, stand, solder, flux, desoldering pump) | 450 | 2 |
| Basic digital multimeter (DT830 type) | 300 | 2 |
| Perfboard and header pins | 100 | 2, 4 |
| Wood, brass strip or hacksaw blade, screws, springs | 150 | 2, 4 |
| MAX9814 microphone module | 200 | 5 |
| USB data cable (not charge only), fitting the ESP32 port | 120 | 2 |
| Total | about 2,840 | |
The owner's Mac has both USB-A and USB-C ports, so any cable that matches the ESP32 port and one of these works. It must be a data cable.
Parts are bought online.
Phase 8 parts are not in this budget and will be planned after licensing.

## Definition of done for any task
1. Code runs and tests pass.
2. The owner has a plain English explanation of what changed and why.
3. `docs/progress-log.md` is updated.
