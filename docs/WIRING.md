# Wiring and pin assignments

The prototype uses the original Waveshare SH1.0 breakout cable and a five-pin
rotary encoder module. The AMOLED connections listed below are internal board
connections; they are not wires that need to be added through the breakout.

## Internal AMOLED interface

| Signal | ESP32-S3 GPIO |
|---|---:|
| Display reset | 8 |
| Chip select | 10 |
| QSPI clock | 11 |
| QSPI data 0 | 12 |
| QSPI data 1 | 13 |
| QSPI data 2 | 14 |
| QSPI data 3 | 15 |
| Tearing-effect signal | 9 |

These assignments match the Waveshare schematic and the current firmware.

## External rotary encoder module

| Encoder label | Connect to | Firmware function |
|---|---|---|
| `GND` | `GND` | Common ground |
| `+` | `3V3` | Encoder module supply |
| `SW` | `GPIO0` | Push button |
| `CLK` | `GPIO1` | Encoder channel A |
| `DT` | `GPIO2` | Encoder channel B |

The encoder module must be powered from 3.3 V, not 5 V. The firmware enables
pull-ups for both quadrature channels and the push button. `SW` is active low.

GPIO0 is also the ESP32-S3 boot-strapping pin and is shared with the board's
BOOT function. Avoid holding the encoder button while powering or resetting the
board, as this may start the ROM download mode instead of the application.

If rotation is reversed on another encoder module, swap `CLK` and `DT` or change
`ENCODER_REVERSED` in the firmware. The current prototype uses the wiring shown
above and does not require either change.

## Additional tactile switches

The housing includes positions for four 6 × 6 × 6 mm tactile switches. They do
not have assigned GPIO pins and are not read by firmware `v0.1.0-poc`. Their
electrical connections will be documented after the final control use case is
selected.

## Notes

- Display bus frequency: 80 MHz
- Encoder mode: full quadrature
- Encoder filter: 1023
- Four transitions are interpreted as one detent
- The current display brightness value is 225

## References

- [Waveshare ESP32-S3-Touch-AMOLED-1.32 documentation](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.32)
- [Official board schematic](https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.32/ESP32-S3-Touch-AMOLED-1.32-Schematic.pdf)

A connector photograph or cable-color diagram will be added after the final
cable route is fixed.
