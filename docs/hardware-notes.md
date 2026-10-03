# Hardware Notes: Circuits, Pin Maps, Parts and Photos

## Pin map
ESP32 DevKit V1, 30 pin. The same table is in CLAUDE.md. Keep both in step.

| Function | GPIO | Notes |
|---|---|---|
| LED | 26 | |
| Sidetone | 25 | |
| Key input | 27 | INPUT_PULLUP, key to GND |
| Touch key and touch paddle dit | 32 | T9 |
| Touch paddle dah | 33 | T8 |
| Paddle dit | 18 | |
| Paddle dah | 19 | |
| OLED SDA | 21 | |
| OLED SCL | 22 | |
| Speed pot | 34 | input only, ADC1 |
| Microphone | 35 | input only, ADC1 |

- Avoid GPIO 0, 2, 12 and 15 for inputs. They are strapping pins: the ESP32
  reads them at power up to decide how to boot, so a key or button on them can
  stop the board from starting.
- Avoid GPIO 1 and 3. They are the USB serial link (TX and RX) to the Mac.
- GPIO 34 and 35 are input only. They have no output driver and no internal
  pull-up. They use ADC1, which keeps working when WiFi is on. ADC2 pins stop
  reading while WiFi is in use, which matters from Phase 3.
- The onboard blue LED is on GPIO 2. It is fine as an output.
- GPIO 21 (SDA) and GPIO 22 (SCL) are the ESP32's usual I2C pins, so the
  OLED needs no extra pin setup.

## Board layout
Two breadboards side by side, tested 2026-10-03 (step 15).

- Why two: the ESP32 DevKit V1 is so wide that on a single breadboard it
  covers nearly every hole beside its pins, leaving almost no room for wires.
  Across two boards, every pin has free holes next to it.
- The new left board has its right rail strip removed, so the two boards
  sit close together.
- The ESP32 is in rows 25 to 40. Its left pins are in column i of the left
  board (free holes f, g, h). Its right pins are in column a of the right
  board (free holes b, c, d, e).
- The right board's right red rail is 5 V (from VIN) and its right blue
  rail is GND. Never put 3.3 V parts like the OLED on that red rail.

ESP32 right side, wires in column c of the right board:

| ESP32 pin | Row | Wire | Goes to |
|---|---|---|---|
| VIN | 26 | c26 | right red rail, 5 V |
| GND | 27 | c27 | right blue rail, GND |
| D27 | 31 | c31 | j58, push button |
| D26 | 32 | c32 | j50, LED resistor |
| D25 | 33 | c33 | row 4 right half, PAM8403 input L |

ESP32 left side, OLED wires in column g of the left board:

| ESP32 pin | Row | Goes to |
|---|---|---|
| 3V3 | 26 | OLED VCC |
| GND | 27 | OLED GND |
| D21 | 36 | OLED SDA |
| D22 | 39 | OLED SCL |

Right board, right half (columns f to j):
- Row 4: the D25 wire, PAM8403 input L, and one leg of the old 330 ohm
  resistor (rows 4 to 6, no longer used).
- Row 8: an old GND wire to the blue rail, no longer used.
- LED: 220 ohm resistor from row 50 to row 54, LED long leg in row 54,
  short leg to the blue rail.
- Button: across the centre gap, legs in rows 58 and 60 (columns e and f),
  j60 to the blue rail.

Right board, left half (columns a to e):
- Row 10: PAM8403 Lout + and speaker +.
- Row 13: PAM8403 Lout - and speaker -.

## Circuits

### External LED on GPIO 26
- Parts: one yellow 5 mm LED and one 220 ohm resistor.
- Wiring: GPIO 26 to the resistor, the resistor to the LED long leg (anode),
  the LED short leg (cathode, flat edge of the rim) to GND. On the two board
  layout: D26 to j50, resistor rows 50 to 54, long leg row 54, short leg to
  the blue rail.
- Current: about (3.3 V minus 2 V across the LED) / 220 ohm, which is about
  6 mA. Bright enough and well within what one GPIO pin can supply.
- Tested 2026-10-02: blinks VU together with the onboard LED on GPIO 2.

### Push button key on GPIO 27
- Parts: one 4-leg tactile push button. No resistor, because the ESP32 has a
  pull-up resistor inside.
- Placement: across the centre gap of the breadboard, legs in rows 58 and 60.
- Wiring: GPIO 27 to row 58, row 60 to GND. On the two board layout: D27 to
  j58, j60 to the blue rail.
- Code: pinMode INPUT_PULLUP, so the pin reads HIGH when open and LOW when
  pressed. 10 ms debounce in software.
- Tested 2026-10-02: both LEDs light while held, exactly one line printed per
  press. Firm quick taps 35 to 55 ms, normal presses 77 to 85 ms, a long
  press 226 ms.

### Sidetone buzzer on GPIO 25
- Parts: one passive magnetic buzzer (measured 16.4 ohm) and one 330 ohm
  resistor (measured 322 ohm).
- Wiring: GPIO 25 to row 4, resistor from row 4 to row 6, buzzer from row 6
  to row 8, row 8 to GND.
- Current: about 9.5 mA peak. With the measured values it is
  3.3 V / (322 + 16.4) ohm, about 9.8 mA. Safe for one GPIO pin.
- Code: LEDC (PWM) channel 0 at 600 Hz, 8 bit duty. Duty 128 for sound,
  duty 0 for silence, which holds the pin LOW.
- Tested 2026-10-03: a 600 Hz tone plays while the key is held, silent when
  released, LEDs follow, nothing gets warm. Very quiet through the resistor.
  2500 Hz was only a little louder, so the low current is the limit.
- Replaced 2026-10-03 by the PAM8403 and speaker below. The buzzer is
  removed. The 330 ohm resistor is still in rows 4 to 6 but no longer used.

### PAM8403 amplifier and speaker for the sidetone
- Parts: PAM8403 module with a B50K volume knob, 8 ohm 1 W speaker
  (measured 7.8 ohm).
- Power: ESP32 VIN to the right red (+) rail, which gives 5 V from USB.
  PAM8403 Power + (white wire) to the red rail, Power - (black wire) to the
  blue GND rail. The right red rail is 5 V, so never use it for 3.3 V parts
  like the OLED.
- Input: PAM8403 input L to row 4 (GPIO 25), input G to the blue GND rail,
  input R unused.
- Output: PAM8403 Lout + to row 10 (a to e) with speaker +, Lout - to row 13
  (a to e) with speaker -.
- Lout - is not GND. The PAM8403 drives both speaker wires, so Lout - must
  never touch the GND rail.
- No extra divider or capacitor on the input. The B50K volume knob and the
  input capacitors on the module handle the 3.3 V square wave from GPIO 25.
- Soldering: header pins soldered on the PAM8403, all neighbouring pairs
  checked with the multimeter for bridges. Speaker wires soldered to its pads.
- Code (from 2026-10-03, step 11): the sidetone uses the ESP32 DAC on
  GPIO 25 (DAC1), not LEDC. A 600 Hz sine wave at 40 kHz from a hardware
  timer, with a 5 ms raised cosine fade in and fade out. Silence is DAC level
  128 (about 1.65 V). At start up the DAC slides from 0 to 128 over 0.5 s.
  The module's input capacitors block this steady 1.65 V, so only the tone
  reaches the speaker.
- Step 10 used LEDC at 600 Hz, duty 128 for sound and duty 0 for silence.
- Tested 2026-10-03: a clear, loud 600 Hz tone while the key is held, silent
  on release, volume knob at about half. Nothing gets hot and the ESP32 does
  not restart.
- Tested 2026-10-03 with the DAC sine sidetone: no thump at start up, a
  smooth soft fade at the start and end of each tone, no clicks from the
  speaker. A very faint hiss when silent, from the 8 bit DAC, fine for now.
  The only click is the tactile button's own mechanical click, confirmed with
  the volume turned fully down.
- Tone colour (from 2026-10-03, step 12): TONE_SHARP, set by TONE_PRESET on
  line 22 of firmware/src/main.cpp. The wave is 1.0 x 600 Hz + 0.33 x 3rd
  (1800 Hz) + 0.20 x 5th (3000 Hz), scaled to fill the DAC range.
- Tested 2026-10-03: all three presets tried on the board. TONE_SHARP kept as
  the brightest, closest to the old square wave. The fade is still smooth
  with no speaker clicks.

### 1.3 inch OLED screen (SH1106, I2C)
- Part: 1.3 inch OLED, 128 x 64 pixels, SH1106 driver, I2C, 4 pins, module
  marked JMD1.3A.
- Address: the select on the back is set to 0x78. That is the 8 bit form of
  the address. The 7 bit form, which most I2C scanners print, is 0x3C.
- Wiring: OLED VCC to ESP32 3V3, GND to GND, SCL to GPIO 22, SDA to GPIO 21.
  VCC must be 3.3 V, never the 5 V rail used by the PAM8403.
- No extra pull-up resistors. The module has its own on SDA and SCL.
- Code (from 2026-10-03, step 15): U8g2 library, constructor
  U8G2_SH1106_128X64_NONAME_F_HW_I2C, which uses the ESP32's own I2C
  hardware. Bus clock 400 kHz. "F" means a full 1 KB picture buffer in RAM.
- Screen layout: "MORSE" in small letters at the top, four lines of decoded
  text in the middle (21 letters each, scrolling up), and the dits and dahs
  of the letter being keyed at the bottom, like ". - .".
- Sending the whole screen takes about 20 to 30 ms. To keep key timing
  exact, the firmware sends it one 8 pixel page per pass of loop(), a few ms
  each, and the key times come from a pin interrupt. See the progress log
  entry for step 15.
