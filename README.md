# Smart Dial

![Project status](https://img.shields.io/badge/status-proof%20of%20concept-247DFE)
![Platform](https://img.shields.io/badge/platform-ESP32--S3-white)
![Display](https://img.shields.io/badge/display-466%C3%97466%20AMOLED-F72545)

Smart Dial is a compact automotive control interface built around a rotary
encoder and a round AMOLED display. It is being developed as a bachelor-project
proof of concept, with the current phase focused on mechanical design, physical
interaction and a responsive user interface.

![Current Smart Dial interface](assets/ui-preview.png)

The prototype uses the
[Waveshare ESP32-S3-Touch-AMOLED-1.32](https://www.waveshare.com/esp32-s3-touch-amoled-1.32.htm),
a custom 3D-printed mechanism and an Arduino_GFX-based interface. The four
current climate pages serve as a realistic interaction demonstrator; the final
control use case will be selected with the project supervisor.

## Highlights

- Fast rotary input with short-press and long-press actions
- Four demonstrator pages: temperature, fan, airflow and seat heating
- Partial display updates for responsive value changes
- Animated fan and clean page transitions
- A trail-free startup sequence built around the OJM Systems logo
- A custom housing, geared rotating ring and screw-mounted display carrier
- A restrained black, white, red and blue visual system

## Hardware

- Waveshare ESP32-S3-Touch-AMOLED-1.32, 466 × 466 pixels
- Five-pin rotary encoder module with an integrated push button
- Four 6 × 6 × 6 mm tactile-switch positions reserved in the housing
- Custom 3D-printed fixed tube, geared ring, base and upper cover
- M3 heat-set inserts and black machine screws for the upper cover

The four additional tactile switches are not assigned in the current firmware.
External connections and internal display pins are documented in
[WIRING.md](WIRING.md).

## Controls

| Action | Result |
|---|---|
| Rotate | Move between pages |
| Short press | Enter or leave value editing |
| Rotate while editing | Change the active value |
| Long press | Turn the climate interface on or off |

## Firmware

The Arduino sketch is contained in one directory:

```text
SmartDial/
├── SmartDial.ino
├── SmartDial_Fonts_DINish.h
└── logomodre256.c
```

### Requirements

- Arduino IDE 2.x
- Espressif ESP32 board package
- **GFX Library for Arduino** (`Arduino_GFX_Library`)
- **ESP32Encoder**
- **LVGL 8.x**, used only to read the generated RGB565 logo asset

### Arduino IDE settings

| Setting | Value |
|---|---|
| Board | ESP32S3 Dev Module |
| USB CDC On Boot | Enabled |
| CPU Frequency | 240 MHz (WiFi) |
| Core Debug Level | None |
| USB DFU On Boot | Disabled |
| Erase All Flash Before Sketch Upload | Disabled |
| Events Run On | Core 1 |
| Flash Mode | QIO 80 MHz |
| Flash Size | 8 MB (64 Mb) |
| Arduino Runs On | Core 1 |
| Partition Scheme | Huge APP (3 MB No OTA / 1 MB SPIFFS) |
| PSRAM | OPI PSRAM |
| Upload Mode | UART0 / Hardware CDC |
| Upload Speed | 921600 |

Open `SmartDial/SmartDial.ino`, select the correct serial port, compile and
upload. A successful startup begins with:

```text
BOOT: Smart Dial v0.1.0-poc
```

## Rendering approach

Normal interaction does not redraw the complete 466 × 466 frame. The firmware
keeps base and working framebuffers in PSRAM, updates only the affected region
and transfers RGB565 data over the 80 MHz QSPI display bus. TE synchronization
is used where it helps prevent visible tearing. Arduino_GFX handles the runtime
interface; LVGL is only needed for the existing logo descriptor.

## Current status

The current firmware baseline is `v0.1.0-poc`. It demonstrates the mechanical
concept, input model and visual direction, but it is not a production automotive
component.

Next milestones are validation of the revised printed housing, selection of the
final control use case and documentation of the assembled prototype. Validated
CAD and print files will be added once the physical fit has been confirmed.

## Project support

The display hardware used for this prototype was provided by Waveshare.
Prototype assembly and bench work have also been supported with tools from:

- Miniware — TS101 Go Kit
- Fanttik — E1 Max precision electric screwdriver
- iFixit — FixMat and Manta Driver Kit

KORAD has confirmed support with a KA3005PS bench power supply. The unit is
currently awaiting shipment and has not been used for this prototype.

Sponsors supplied hardware or tools and did not control the design, firmware or
project conclusions.

## License

No source-code or hardware license has been selected yet. Until a license is
added, the project remains under default copyright while publication terms are
being confirmed with the project supervisor.

The bundled DINish font data is separately distributed under the SIL Open Font
License 1.1. See [licenses/DINish-OFL-1.1.txt](licenses/DINish-OFL-1.1.txt).

