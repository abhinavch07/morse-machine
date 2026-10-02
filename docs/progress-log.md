# Progress Log

## 2026-09-27 Phase 1, session 1: project setup

Built:
- Checked tools: Python 3.13.15 and git 2.51.0 are installed.
- Created the project virtual environment in .venv using python3.13.
- Created the repo folder layout from CLAUDE.md.
- Added docs files, a dev requirements file, a .gitignore, a README and an MIT licence.
- Installed pytest inside the venv.

Learned:
- A virtual environment keeps this project's Python packages separate from the system Python.
- Git does not track empty folders, so some phase folders appear later when they hold files.

Next:
- Start Phase 1 core logic: text to Morse encoding with tests.

## 2026-09-27 Phase 1, session 1 part 2: Morse encode and decode

Built:
- morse_core/codes.py: the ITU code table for letters, figures, the ASOC
  punctuation and the prosigns AR, SK, BT, KN. The break sign "=" and <BT>
  share the code -...-.
- morse_core/translate.py: encode(text) and decode(morse). Encode accepts
  lowercase and messy spacing. Both raise a clear ValueError on unknown input.
- tests/test_translate.py: 25 checks including SOS, PARIS, full round trips of
  letters and figures, each punctuation mark and prosign, lowercase, extra
  spaces and error cases. All pass.

Learned:
- Some Morse codes are shared. The break sign "=" and the prosign <BT> are the
  same code, so decoding cannot tell them apart. We chose to decode -...- to
  the printable "=" and documented that rule.
- Prosigns are sent as one run of dits and dahs, so we write them in text
  inside angle brackets like <AR> and parse them as one token.

Next:
- Add the timing engine (dit length from WPM, Farnsworth spacing).

## 2026-09-27 Phase 1, session 2: timing engine and WAV output

Built:
- morse_core/timing.py: dit_length_ms, compute_timing (standard and
  Farnsworth), text_to_key_events (list of key_down and duration_ms with no
  trailing gap), and total_duration_ms (adds one final word gap).
- morse_core/audio.py: sine sidetone at 600 Hz, 44100 sample rate, 16-bit mono
  WAV via the standard library wave module, with a 5 ms raised cosine fade in
  and out on every tone to kill clicks. No numpy needed.
- morse_core/make_wav.py: command line tool, for example
  python -m morse_core.make_wav "PARIS PARIS" --wpm 20 --effective 8 --out paris.wav
- tests/test_timing.py and tests/test_audio.py: 13 new checks. All 38 tests
  pass.

Learned:
- The PARIS word slot is 50 units. That is why 20 WPM standard gives 3000 ms
  and 20/8 Farnsworth gives 7500 ms for one word.
- Farnsworth keeps the characters fast (a dit stays 60 ms at 20 WPM) but
  stretches the gaps: letter gap about 891 ms and word gap about 2078 ms.
- A raised cosine fade of a few ms on each tone removes the click you hear when
  a sine wave starts or stops abruptly.

Next:
- Live audio playback, or start the Koch trainer in trainer/.

## 2026-09-27 Phase 1, session 3: Koch trainer

Built:
- trainer/koch.py: the LCWO Koch order (41 characters, 40 lessons) and
  lesson_characters(n) for the first n+1 characters. Order checked and
  confirmed against LCWO.
- morse_core/codes.py: added "/" (-..-.) which the Koch order needs.
- trainer/scoring.py: LCWO style Levenshtein scoring with a per character
  error count and a list of wrong characters.
- trainer/groups.py: random groups of five, about one minute in total, with the
  newest character given double weight.
- trainer/progress.py: save and load data/progress.json, the 90 percent pass
  rule that moves the start lesson up, and a weakest characters summary.
- trainer/practice.py: the terminal round. Plays a temp WAV with macOS afplay
  and deletes it, reads your copy, scores and saves it.
- Tests: test_koch, test_scoring, test_groups, test_progress. 21 new checks,
  59 total pass.
- data/ added to .gitignore so progress stays local.

Learned:
- The Koch method adds one character per lesson at full character speed, and
  Farnsworth spacing gives the ears time between characters.
- Levenshtein scoring is fair because one missed character does not wreck the
  rest of the line, unlike a strict position by position match.

Next:
- Try a real practice round with sound, then look at live audio playback or a
  richer fist analyser.

## 2026-09-27 Session wrap and plan update

State:
- Phase 1 software is complete and pushed to GitHub: encode and decode, the
  timing engine (standard and Farnsworth), WAV output, and the Koch trainer.
  59 tests pass.
- Updated CLAUDE.md for Phase 2 hardware. Added a 1.3 inch SH1106 OLED (I2C,
  128x64, U8g2 library) to the goal and done-when, with a note that the driver
  is SH1106 and not SSD1306. Revised the parts list to an ESP32 DevKit V1 with
  CP2102, two 10k pots, a USB data cable line, new total about Rs 2,840.

Next:
- Phase 0 daily ear practice: python -m trainer.practice.
- When ready for more code: live audio playback or a fist analyser.
- Phase 2 hardware once the parts arrive. First soldering and I2C for the OLED.




## 2026-10-02 Phase 2 session 1: first ESP32 program

Built:
- PlatformIO project in firmware/ for the DOIT ESP32 DevKit V1 (Arduino
  framework). Upload and monitor port /dev/cu.usbserial-0001 at 115200 baud.
- firmware/src/main.cpp prints a hello line, then blinks the onboard blue LED
  on GPIO 2 (500 ms on, 500 ms off) and prints "LED on" and "LED off".
- .pio/ added to .gitignore. The build passes (RAM 6.6 percent, flash 20.5
  percent). Upload is done by hand from VS Code.

Learned:
- setup() runs once at power up or reset. loop() then runs forever.
- The serial monitor speed must match Serial.begin(), or the text is garbage.
- delay() freezes the whole program. That is fine for a blink but not for
  reading a key, so later sessions will switch to millis() timing.

Next:
- Upload from VS Code and watch the serial monitor.
- Then wire a tactile push button on the breadboard and read it with debounce.
