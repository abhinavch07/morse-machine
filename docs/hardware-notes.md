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
