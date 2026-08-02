# Wiring and pin assignments

The Waveshare board carries the display interface internally. These are the pin
assignments used by the current firmware and are recorded here mainly to make
the source easier to audit.

## AMOLED interface

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

## Rotary encoder

| Signal | ESP32-S3 GPIO |
|---|---:|
| Encoder A | 1 |
| Encoder B | 2 |
| Encoder push button | 0 |

The button uses the internal pull-up and is active low. GPIO0 also participates
in the ESP32-S3 boot process, so avoid holding the encoder button while powering
or resetting the board.

## Notes

- Display bus frequency: 80 MHz
- Encoder mode: full quadrature
- Encoder filter: 1023
- Four transitions are interpreted as one detent
- The current display brightness value is 225

Add photographs or a simple connection diagram here once the final cable route
and external button wiring are fixed.

