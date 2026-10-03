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

## Circuits

### External LED on GPIO 26
- Parts: one yellow 5 mm LED and one 220 ohm resistor.
- Wiring: GPIO 26 to the resistor, the resistor to the LED long leg (anode),
  the LED short leg (cathode, flat edge of the rim) to GND.
- Current: about (3.3 V minus 2 V across the LED) / 220 ohm, which is about
  6 mA. Bright enough and well within what one GPIO pin can supply.
- Tested 2026-10-02: blinks VU together with the onboard LED on GPIO 2.

### Push button key on GPIO 27
- Parts: one 4-leg tactile push button. No resistor, because the ESP32 has a
  pull-up resistor inside.
- Placement: across the centre gap of the breadboard, legs in rows 58 and 60.
- Wiring: GPIO 27 to row 58, row 60 to GND.
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
- Code: unchanged from the buzzer step. LEDC at 600 Hz, duty 128 for sound,
  duty 0 for silence.
- Tested 2026-10-03: a clear, loud 600 Hz tone while the key is held, silent
  on release, volume knob at about half. Nothing gets hot and the ESP32 does
  not restart.
