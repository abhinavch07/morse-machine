# Progress Log

## Known issues

- Decoder speed tuning parked on 3 Oct 2026. Some SOS sending still gives *.
  Limits now: split 160 ms, letter gap 250 ms, word gap 2500 ms. Plan:
  adaptive speed later, or retune from fresh debug logs.

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
- Tested on real hardware: the upload from VS Code worked, the blue LED on
  GPIO 2 blinks, and the serial monitor shows the hello line and the
  "LED on" and "LED off" messages.

Learned:
- setup() runs once at power up or reset. loop() then runs forever.
- The serial monitor speed must match Serial.begin(), or the text is garbage.
- delay() freezes the whole program. That is fine for a blink but not for
  reading a key, so later sessions will switch to millis() timing.

Next:
- Wire a tactile push button on the breadboard and read it with debounce.

## 2026-10-02 Phase 2 session 1 step 5: Morse on the onboard LED

Built:
- firmware/lib/morse: a small C++ Morse library that copies the rules of
  morse_core. Same table (letters, figures, ASOC punctuation and /), dit =
  1200 / WPM, dah 3 units, gaps of 1, 3 and 7 units. No Farnsworth and no
  prosigns yet.
- firmware/test/test_morse: 8 PlatformIO unit tests that run on the Mac with
  pio test -e native. They check the same numbers as the Python tests: dit
  60 ms at 20 WPM and 240 ms at 5 WPM, V is ...- and U is ..-, PARIS at
  20 WPM is 3000 ms. All 8 pass. The 59 Python tests still pass.
- main.cpp now blinks "VU" at 5 WPM on GPIO 2, waits 2 seconds and repeats,
  printing "V ...-" and "U ..-" over serial. The ESP32 build passes.
- Tested on real hardware: the native tests pass on the Mac, and after the
  upload from VS Code the blue LED on GPIO 2 blinks VU correctly.

Learned:
- A library with no Arduino code in it can be tested on the Mac, which is much
  faster than uploading to the board each time.
- C++ on the ESP32 has no exceptions, so bad input returns 0 or nullptr where
  Python would raise an error.

Next:
- Wire a tactile push button on the breadboard and read it with debounce.
- Before the Phase 2 decoder work, add the prosigns AR, SK, BT and KN to the
  C++ library, with tests, so it matches morse_core. BT shares -...- with "=",
  and Python decodes that code to "=", so the C++ decoder must do the same.

## 2026-10-02 Phase 2 session 1 step 7: external LED

Built:
- Added a pin map to CLAUDE.md and docs/hardware-notes.md: LED 26,
  sidetone 25, key 27, touch 32 and 33, paddle 18 and 19, OLED 21 and 22,
  speed pot 34, microphone 35. Notes on pins to avoid and on ADC1.
- main.cpp blinks "VU" on the onboard LED (GPIO 2) and the external LED
  (GPIO 26) at the same time. Pin names are constants that match the pin map.
  The ESP32 build and the 8 native tests pass.
- Tested on real hardware: both LEDs blink VU together. The external LED is
  yellow with a 220 ohm resistor, about 6 mA. Circuit noted in
  docs/hardware-notes.md.

Learned:
- Strapping pins (0, 2, 12, 15) are read at power up to choose the boot mode,
  so inputs go elsewhere. GPIO 1 and 3 carry the USB serial link.
- Pins on ADC2 cannot read analog values while WiFi is on, so the pot and the
  microphone go on ADC1 pins 34 and 35.

Next:
- Wire a tactile push button on GPIO 27 and read it with debounce.
- Before the Phase 2 decoder work, add the prosigns to the C++ library.

## 2026-10-02 Phase 2 session 1 step 8: push button key

Built:
- main.cpp reads a push button on GPIO 27 (KEY_PIN) with INPUT_PULLUP, the
  other side of the button to GND. LOW means pressed.
- 10 ms debounce using millis(). While the button is held both LEDs (GPIO 2
  and 26) are on. On release it prints the hold time, like "key down 85 ms".
- VU blinking is stopped, but sendText() stays in the file for later.
- The ESP32 build and the 8 native tests pass.
- Tested on real hardware: both LEDs light while the button is held, and
  there is exactly one line per press, so the debounce works. Firm quick taps
  read 35 to 55 ms, normal presses 77 to 85 ms, a long press 226 ms. Wiring
  noted in docs/hardware-notes.md.

Learned:
- INPUT_PULLUP connects a resistor inside the ESP32 from the pin to 3.3 V, so
  the pin reads HIGH when the button is open and LOW when it is pressed.
- A metal contact bounces for a few ms. Waiting for the reading to stay the
  same for 10 ms turns the bounces into one clean press.
- loop() with no delay() runs thousands of times a second, so it never misses
  a press. This is why the key code uses millis() and not delay().

Next:
- Then the sidetone on GPIO 25.
- Before the Phase 2 decoder work, add the prosigns to the C++ library.

## 2026-10-03 Phase 2 session 2 step 9: sidetone on a passive buzzer

Built:
- Wiring: GPIO 25 to a 330 ohm resistor, then a passive magnetic buzzer
  (measured 16.4 ohm) to GND. Peak current about 3.3 V / 346 ohm = 9.5 mA.
- main.cpp plays a 600 Hz tone on GPIO 25 while the key on GPIO 27 is held,
  together with both LEDs. The 10 ms debounce and the "key down N ms" print
  are unchanged.
- The tone uses the ESP32 LEDC (PWM) hardware: set up once at 600 Hz with
  8 bit duty, then duty 128 (50 percent) for sound and duty 0 for silence.
  Duty 0 holds the pin LOW, so there is no hum and no current when key up.
- Named constants SIDETONE_PIN = 25 and SIDETONE_HZ = 600.
- The ESP32 build and the 8 native tests pass.
- Tested on real hardware: a 600 Hz tone plays while the key is held, silent
  when released, LEDs follow, nothing gets warm. Wiring noted in
  docs/hardware-notes.md.
- The tone is very quiet through the 330 ohm resistor. A hand test at 2500 Hz
  was only a little louder, so the low current is the limit, not the pitch.
  Set back to 600 Hz.

Learned:
- The ESP32 pin can only be fully on or fully off. Switching it 600 times a
  second makes the buzzer move 600 times a second, which we hear as a tone.
- The LEDC hardware makes the square wave by itself, so loop() stays free to
  read the key.
- The installed framework is Arduino-ESP32 2.0.17, so it uses the channel
  based calls ledcSetup(), ledcAttachPin() and ledcWrite().

Next:
- PAM8403 amplifier and 8 ohm speaker on the sidetone, to make it louder.
- The 5 ms rise and fall ramp from the spec is not done yet. A square wave on
  a buzzer cannot fade smoothly, so it fits better with the PAM8403 speaker.
- Before the Phase 2 decoder work, add the prosigns to the C++ library.

## 2026-10-03 Phase 2 session 2 step 10: PAM8403 amplifier and speaker

Built:
- First soldering session. Header pins soldered on the PAM8403 module, then
  every neighbouring pair of pins checked with the multimeter for solder
  bridges. Wires soldered to the speaker pads.
- Sidetone moved from the buzzer to a PAM8403 amplifier and an 8 ohm 1 W
  speaker (measured 7.8 ohm). The buzzer is removed.
- The PAM8403 runs on 5 V from the ESP32 VIN pin through the right red rail.
  Its input L takes GPIO 25 directly, through its B50K volume knob.
- No code change. The step 9 LEDC sidetone drives the amplifier as it is.
- Tested on real hardware: a clear, loud 600 Hz tone while the key is held,
  silent on release, volume knob at about half. Nothing gets hot and the
  ESP32 does not restart. Wiring noted in docs/hardware-notes.md.

Learned:
- How to solder header pins, and to check every pair for bridges with the
  multimeter before powering up.
- The ESP32 VIN pin gives the 5 V from USB, for parts that need more than
  3.3 V.
- Lout - on the PAM8403 is not GND. The amplifier drives both speaker wires,
  so the speaker must connect only to Lout + and Lout -.
- The quiet buzzer was limited by the current a GPIO pin can give. The
  amplifier takes its power from 5 V instead, so the pin only has to send
  the signal.

Next:
- The 5 ms rise and fall ramp from the spec, now that the speaker can show
  any clicks.
- Before the Phase 2 decoder work, add the prosigns to the C++ library.

## 2026-10-03 Phase 2 session 2 step 11: sine wave sidetone with 5 ms ramp

Built:
- Checked the core: this PlatformIO project uses Arduino-ESP32 2.0.17. So the
  timer uses timerBegin(num, divider, countUp), timerAttachInterrupt,
  timerAlarmWrite and timerAlarmEnable. The DAC is switched on with dacWrite()
  in setup() only. The timer interrupt writes the DAC1 register directly
  (8 bits at bit 19 of RTC_IO_PAD_DAC1_REG), which is fast and IRAM-safe.
- IRAM safety: onSampleTimer() and nextSample() are IRAM_ATTR, the register
  write is inline, the sine and fade tables are arrays inside a global object
  in RAM, and toneOn is volatile. Checked in the built firmware: both
  functions sit in IRAM, the tables in RAM, and the interrupt calls nothing
  in flash. The divide by 255 became a multiply, so no hidden helper call.
- firmware/lib/sidetone: envelopeRise() and envelopeFall() (raised cosine,
  the same shape as audio.py) and a SidetoneGenerator that makes one 8 bit DAC
  value per sample from a 256 step sine table and a 200 step envelope table.
  Silence is the middle level, 128. No floats in nextSample(), because floats
  are not safe inside an ESP32 interrupt.
- firmware/test/test_sidetone: 6 native tests. The envelope is 0 at 0 ms,
  half at 2.5 ms and full at 5 ms, and the reverse for the fall. Silence is
  always 128, the tone starts gently and reaches the full range after 5 ms,
  it is back at exactly 128 5 ms after release, and one second of samples has
  600 waves. All 14 native tests pass.
- main.cpp: LEDC removed from GPIO 25. Hardware timer 0 runs at 40 kHz and
  writes each sample to DAC1 (GPIO 25). At start up the DAC slides from 0 to
  128 over 0.5 s to avoid a thump. Debounce, LEDs and the "key down N ms"
  print are unchanged.
- The ESP32 build passes.
- Tested on real hardware: no thump at start up, and the tone starts and
  stops with a smooth soft fade, no clicks from the speaker. A very faint
  hiss when silent, fine for now.
- The only click left is the tactile button's own mechanical click. Turning
  the volume fully down confirmed it does not come from the speaker.
- The sine sounds softer than the old square wave. The owner prefers a
  slightly brighter tone, details to follow.

Learned:
- A DAC turns a number into a voltage. The ESP32 DAC is 8 bit, so 0 to 255
  gives about 0 to 3.3 V.
- A sine table plus a phase counter makes any pitch. At 40 kHz and 600 Hz,
  each wave is about 67 samples long.
- The raised cosine envelope fades the tone in and out, which removes the
  click that a sudden start or stop makes.

Next:
- Maybe attach the timer with ESP_INTR_FLAG_IRAM so the tone keeps playing
  while flash is busy. Check the whole interrupt chain first.
- Before the Phase 2 decoder work, add the prosigns to the C++ library.

## 2026-10-03 Phase 2 session 2 step 12: brighter sidetone with harmonics

Built:
- lib/sidetone: three tone presets, TONE_SOFT (pure sine), TONE_BRIGHT
  (1.0 x fundamental + 0.25 x 3rd + 0.10 x 5th) and TONE_SHARP (1.0 + 0.33 x
  3rd + 0.20 x 5th). begin() adds the waves, finds the peak and scales the
  table so the peak is exactly 127, so 128 plus any entry stays in 1 to 255.
- main.cpp: one constant, TONE_PRESET = TONE_BRIGHT, picks the preset. The
  table is built once in setup() and stays in RAM. The 5 ms fade, the DAC,
  600 Hz and the IRAM-safe interrupt are unchanged. Checked in the built
  firmware: the interrupt code is still in IRAM and calls nothing in flash.
- Native tests now run every check for all three presets: table inside 0 to
  255 with a peak of exactly 127, the right amount of 3rd and 5th harmonic,
  silence is 128, the fade in starts at 128, the fade out ends at 128, and
  one second has 600 waves. All 16 native tests pass. The ESP32 build passes.
- Tested on real hardware: all three presets tried. Keeping TONE_SHARP, so
  line 22 of main.cpp is now TONE_PRESET = TONE_SHARP. The fade is still
  smooth with no speaker clicks. Choice noted in docs/hardware-notes.md.

Learned:
- Harmonics are extra waves at whole number multiples of the pitch. They do
  not change the note, they change its colour. More high harmonics sound
  brighter.
- A square wave is a sine plus 1/3 of the 3rd, 1/5 of the 5th and so on.
  TONE_SHARP is close to the start of that recipe.
- Odd harmonics in this phase flatten the top of the wave, so the peak gets
  lower (0.85 for BRIGHT, 0.93 for SHARP). After scaling, the 600 Hz part is
  about 1.18 and 1.08 times bigger, so the tone is also a little louder.

Next:
- Before the Phase 2 decoder work, add the prosigns to the C++ library.

## 2026-10-03 Phase 2 session 2 step 13: prosigns in the C++ Morse library

Built:
- lib/morse: the table now uses text tokens ("A", "<AR>") instead of single
  chars, so prosigns fit. It has the same entries in the same order as
  morse_core/codes.py, including <AR>, <SK>, <BT> and <KN>.
- New morseCodeAt(text, &used): reads one token from the start of the text,
  either one character or a prosign in angle brackets, and says how many
  chars it used. Lowercase works, like Python.
- New morseDecode(code): code in, text out, nullptr for an unknown code.
  The shared code -...- decodes to "=", the same rule as Python.
- totalDurationMs() now counts a prosign as one character, so "<AR>" has no
  letter gaps inside. It gives 1200 ms at 20 WPM, the same as Python.
- morseCode(char) still works, so main.cpp did not change.
- 5 new native tests. All 21 native tests pass. The ESP32 build passes.
- Matching Python test in tests/test_timing.py: "<AR>" totals 1200 ms and
  "AR" totals 1320 ms at 20 WPM. All 60 Python tests pass.

Learned:
- A prosign is two letters run together with no letter gap, so it sounds
  like one character. <AR> takes 20 units, while A then R takes 22.
- Python and C++ differ on bad input. Python raises an error. C++ on the
  ESP32 has no exceptions, so it returns nullptr and skips: an unknown
  prosign like <ZZ> is skipped whole, and a "<" with no ">" in the same word
  is skipped on its own.

Next:
- Use morseCodeAt() in main.cpp so the ESP32 can send prosigns.
- Then the Phase 2 decoder: time key presses, build a code, and use
  morseDecode() to show the character on serial and the OLED.

## 2026-10-03 Phase 2 session 2 step 14: decode key presses on serial

Built:
- lib/key_decoder: a KeyDecoder with no Arduino code. keyDown(ms) adds a
  dit (under 2 units) or a dah (2 units or more). keyUp(ms) is called again
  and again while the key is up. At 2 units it finishes the letter, at 5
  units it finishes the word, and each is reported only once per gap. It
  uses morseDecode(), and an unknown code comes out as "*".
- Receive speed is fixed at RX_WPM = 15, so 1 unit is 80 ms. A dah must be
  at least 160 ms, a letter ends after 160 ms up, a word after 400 ms up.
- main.cpp: sidetone, LEDs and debounce are unchanged. The "key down N ms"
  print is gone. Letters print on one line as soon as they are decoded,
  with no code: prosigns as their text like <AR>, unknown codes as "*". A
  word gap prints one space. After 3 seconds with the key up, a new line
  starts, but only if something was printed since the last new line.
- 9 new native tests: K, VU, <AR>, a word gap between letters, uneven human
  timing (CQ), the letter showing at exactly 2 units while the key is up,
  no output before the first press, and unknown codes as "*". All 30 native
  tests pass. The ESP32 build passes.
- Board test of the first version: K, M and U decoded correctly. The output
  had one letter and its code per line, which did not read like words, so
  the printing was changed to the one line form above.
- Second board test: SOS sent slowly printed "S MT S". Presses were about
  15 WPM, but a gap inside O was over 160 ms, so O split into M and T, and
  the letter gaps were over 400 ms, so they printed as word spaces.
- Added DEBUG_TIMING = true in main.cpp (temporary). It prints "down N" for
  each press and "up N" for each gap, in ms, so the real timings can be
  measured. The decoder is unchanged.
- Timing test of the owner's hand, two SOS groups (ms): dits 45 to 139,
  dahs 180 to 325, gaps inside a letter 58 to 136, gaps between letters
  427, 618, 1370 and 1458, pause between the two groups 3822. SOS decoded,
  but a space printed after every letter, because every letter gap was over
  the 400 ms word limit. (A first reading took 1370 and 1458 for pauses
  between groups. They were really gaps between S and O.)
- The decoder now takes three limits in ms instead of one WPM:
  DIT_DAH_SPLIT_MS = 160, LETTER_GAP_MS = 250, WORD_GAP_MS = 2500. They are
  named constants in main.cpp. Each sits in the empty space between two of
  the measured ranges. NEW_LINE_MS went from 3000 to 5000, so a new line
  does not start inside a normal pause.
- Native decoder tests updated for the new limits, plus a test that feeds
  the real SOS SOS log and must give "SOS SOS" with one space between the
  groups, and a test of the exact edge of each limit. All 33 native tests
  pass. The ESP32 build passes.
- Board test with these limits: SOS mostly decodes, but sometimes gives *.
  Speed tuning is parked, see Known issues at the top of this file.
- DEBUG_TIMING set to false for normal use. Set it to true to see the
  press and gap times again.

Learned:
- A decoder only needs to measure time. Short press or long press gives dit
  or dah. Short gap, medium gap or long gap says what comes next.
- Putting the cut offs halfway between the ideal lengths (2 units between a
  1 unit dit and a 3 unit dah) gives room for an uneven fist.
- A real hand is not a textbook. Mine sends presses near 15 WPM but leaves
  gaps of up to 1.5 seconds between letters, which is natural Farnsworth
  spacing. Measuring first, then putting each limit in the empty space
  between two ranges, works better than guessing a speed.

Next:
- Show the decoded text on the OLED.
- Later: adaptive speed in the decoder, or retune from fresh debug logs.

## 2026-10-03 Phase 2, session 2 step 15: decoded text on the OLED

Built:
- Wired the 1.3 inch SH1106 OLED (JMD1.3A, address 0x78) to 3V3, GND,
  GPIO 21 (SDA) and GPIO 22 (SCL). The ESP32 now sits across two
  breadboards.
- Added the U8g2 library to platformio.ini, for the ESP32 build only.
- lib/screen_text: a ScreenText class with no Arduino code. It keeps 4 lines
  of 21 letters, moves a word down whole when the line is full, never splits
  a prosign like <AR>, skips extra spaces and scrolls the oldest line off the
  top. spacedCode() turns ".-." into ". - .".
- KeyDecoder.currentCode(): the dits and dahs of the letter in progress, or
  "" when there is none.
- main.cpp: the screen shows "MORSE" at the top, the decoded text in the
  middle, and the code being keyed at the bottom. It is redrawn only when a
  letter, a word gap, a new line or a new dit or dah arrives. The 5 second
  pause that starts a new line on serial also starts one on the screen.
  Serial printing is unchanged.
- Timing: a pin interrupt on GPIO 27 notes the time of every edge. The
  debounce in loop() still waits 10 ms of quiet, but the press and gap times
  now come from the interrupt's edge time, not from when loop() noticed. The
  screen is sent one 8 pixel page per pass of loop(), so loop() never stops
  for more than a few ms. The sidetone timer interrupt is untouched.
- With DEBUG_TIMING = true, start up also prints how long a full screen and
  one page take to send.
- 11 new native tests (10 for ScreenText, 1 for currentCode). All 44 native
  tests pass. The ESP32 build passes.
- Board test passed: MORSE shows at start up, the bottom line shows the
  dits and dahs while keying, and the decoded text matches serial.
- hardware-notes.md now has the two breadboard layout, tested 3 Oct 2026:
  ESP32 in rows 25 to 40, every wire by row and column, and the OLED wires
  on the left board.

Learned:
- I2C sends data one bit at a time on two wires. At 400 kHz the 1 KB screen
  takes about 25 ms, which is longer than the room between my dits and dahs.
- An interrupt can save the exact time of an event even when the main loop
  is busy, as long as it does very little, here just one line.
- Sharing a value between an interrupt and loop() needs care: read it,
  check it again, and skip the pass if it changed in between.
- I2C addresses come in two forms. 0x78 is 0x3C moved one bit left.

Next:
- Optional: turn on DEBUG_TIMING once to read the real screen send times.
- Later: adaptive speed in the decoder, or retune from fresh debug logs.
- Then the DIY straight key, and after it the touch key on GPIO 32.

## 2026-10-03 Phase 2, session 2 step 16: touch key on GPIO 32

Built:
- Hardware: a 2 cm foil pad on a short jumper in c35, next to D32 (T9).
- lib/touch_key: a TouchKey class with no Arduino code. It averages
  calibration readings into a no touch level, then turns raw readings into
  touched or not touched with two limits (touched below 65%, released above
  80%) and only changes after 3 readings in a row agree. It also remembers
  when the change began, so the smoothing delay does not change timings.
- main.cpp: at start up it prints "Touch calibrating, do not touch the pad",
  averages 2 seconds of readings and prints the level and both limits. In
  loop() the pad is read every 3 ms with touchRead(). Key down is button OR
  pad, and both go to the same decoder, sidetone, LEDs and OLED through one
  function, updateKey(). The button path and its interrupt timing are the
  same as before.
- The touch hardware now measures every 2.2 ms instead of every 27 ms,
  using touchSetCycles(). It has to be called after the first touchRead(),
  because switching the hardware on resets it (with the two numbers
  swapped, in core 2.0.17).
- DEBUG_TOUCH = false. When true it prints "touch N" about 10 times a second.
- 8 new native tests: calibration average, no touch without calibration,
  press below 65%, no release until above 80%, no flicker around one limit,
  single noisy readings ignored, change time, and the changing flag. All 52
  native tests pass. The ESP32 build passes.
- Board test passed: the pad turns the tone and LEDs on and off cleanly
  with no flicker. K decodes from both the pad and the button, and the
  button works as before.
- Calibration at the test: no touch level 78.6, touched below 51.1, released above 62.9.

Learned:
- Capacitive touch counts how fast the pad charges. A finger is extra
  capacitance, so the count drops.
- Hysteresis: one limit flickers when a reading wobbles around it. Two
  limits with a gap between them give a dead zone where nothing changes.
- Reading a sensor faster than it measures just gives the same old number
  again. The default touch timing would have added about 27 ms per reading.

Next:
- Optional: use DEBUG_TOUCH once to see how far a touch drops below the
  press limit.
- Build the DIY straight key, then later the touch paddle on GPIO 32 and 33.

