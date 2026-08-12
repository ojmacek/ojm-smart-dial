# Smart Dial

![Project status](https://img.shields.io/badge/status-CAN%20proof%20of%20concept-247DFE)
![Platform](https://img.shields.io/badge/platform-ESP32--S3-white)
![Display](https://img.shields.io/badge/display-466%C3%97466%20AMOLED-F72545)
![CAN](https://img.shields.io/badge/CAN-500%20kbit%2Fs-1687FF)

Smart Dial is a compact automotive control interface built around a rotary
encoder and a round AMOLED display. It is developed as a bachelor’s project at VSB - Technical University of Ostrava as a
proof of concept focused on mechanical design, physical interaction, responsive
graphics and communication with a simulated vehicle over CAN.

![Current Smart Dial interface](assets/ui-preview.png)

[Watch the current working prototype demonstration](https://www.youtube.com/watch?v=JF2uquMCpn4&t=3s)

The prototype uses the
[Waveshare ESP32-S3-Touch-AMOLED-1.32](https://www.waveshare.com/esp32-s3-touch-amoled-1.32.htm),
a custom 3D-printed mechanism, an Arduino_GFX-based interface and an
SN65HVD230 CAN transceiver. The current climate pages provide a realistic
interaction demonstrator while the final control functions are still being
defined.

The present milestone completes one full interaction path: changes made with
the physical dial are transmitted over CAN to a Windows vehicle ECU simulator,
and commands issued by the simulator are received and reflected on the display.

## Highlights

- Fast rotary input with short-press and long-press actions
- Four demonstrator pages: temperature, fan, airflow and seat heating
- Partial display updates for responsive value changes
- Animated fan, clean page transitions and a trail-free startup sequence
- A custom housing, geared rotating ring and screw-mounted display carrier
- A restrained black, white, red and blue visual system
- Bidirectional Classical CAN communication at 500 kbit/s
- Periodic dial-state transmission every 200 ms
- Remote commands with state confirmation and automatic retry
- A Windows vehicle ECU simulator with live state, diagnostics and event logging

## System overview

```text
Rotary encoder
      │
      ▼
Smart Dial ── SN65HVD230 ── CAN bus ── CANable 2.0 ── Vehicle ECU Simulator
      ▲                                                        │
      └──────────────────── command confirmation ──────────────┘
```

The CANable interface and PC application are bench-development tools. The
current prototype communicates with a simulated vehicle and is not intended for
connection to a production vehicle.

## Hardware

- Waveshare ESP32-S3-Touch-AMOLED-1.32, 466 × 466 pixels
- Five-pin rotary encoder module with an integrated push button
- SN65HVD230 3.3 V CAN transceiver
- CANable 2.0 USB-to-CAN adapter for PC testing
- Four 6 × 6 × 6 mm tactile-switch positions reserved in the housing
- Custom 3D-printed fixed tube, geared ring, base and upper cover
- M3 heat-set inserts and black machine screws for the upper cover

The four additional tactile-switch positions are not assigned in the current
firmware. External connections and internal display pins are documented in
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

The ESP32 board package provides the TWAI driver used for CAN communication;
no additional CAN library is required.

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
upload. A successful startup reports:

```text
BOOT: Smart Dial PREMIUM SNAPPY V5.5 CAN
CAN: ready | 500 kbit/s | TX 0x201 | RX 0x301
```

## CAN communication

The proof of concept uses Classical CAN with standard 11-bit identifiers at
500 kbit/s.

- `0x201` — eight-byte dial state, transmitted every 200 ms
- `0x301` — two-byte command sent by the simulated vehicle ECU

The state frame contains power, A/C, temperature, fan, airflow, seat heating,
visible page, alive counter and an XOR checksum. The next periodic state frame
also confirms that a received command has been applied.

The complete byte layout, signal scaling and example messages are documented in
[CAN_PROTOCOL.md](CAN_PROTOCOL.md).

## Vehicle ECU simulator

The included Windows application monitors the live dial state and sends vehicle
commands back over a CANable-compatible SLCAN connection.

```text
VehicleEcuSimulator/
├── OJM_Vehicle_ECU_Simulator.py
├── OJM_Logo.png
├── OJM_Splash.png
└── BUILD_EXE.bat
```

### Running from Python

Python 3.10 or newer is required. Install the dependency with:

```powershell
py -m pip install "python-can[serial]"
```

Run the simulator from the repository root:

```powershell
py VehicleEcuSimulator/OJM_Vehicle_ECU_Simulator.py
```

The default SLCAN port is `COM5`. A different port can be supplied with:

```powershell
py VehicleEcuSimulator/OJM_Vehicle_ECU_Simulator.py --port COM6
```

Close Cangaroo or any other application using the CANable serial port before
connecting the simulator.

### Building the Windows executable

Run `VehicleEcuSimulator/BUILD_EXE.bat`. The script installs or updates
PyInstaller and creates:

```text
VehicleEcuSimulator/dist/OJM_Vehicle_ECU_Simulator.exe
```

The simulator waits for a valid `0x201` frame before enabling remote commands.
It monitors frame integrity and the alive counter, reports the dial offline
after 800 ms without a valid state frame, and retries unconfirmed commands up
to three times.

## Rendering approach

Normal interaction does not redraw the complete 466 × 466 frame. The firmware
keeps base and working framebuffers in PSRAM, updates only the affected region
and transfers RGB565 data over the 80 MHz QSPI display bus. TE synchronization
is used where it helps prevent visible tearing. Arduino_GFX handles the runtime
interface; LVGL is only needed for the existing logo descriptor.

CAN processing is non-blocking and limited per main-loop pass so bus traffic
cannot monopolise input or display updates. Bus-off recovery is handled without
stopping the user interface.

## Current status

The current public milestone is `v0.2.0-can-poc`. It demonstrates the mechanical
concept, responsive interface and bidirectional communication between one
physical dial and the vehicle ECU simulator.

The next phase will define the final functions of three physical dials, assign
separate CAN identifiers and introduce configurable left, centre and right dial
roles. Additional displays are not required to continue protocol and software
development; the current single-dial implementation remains the validated
baseline.

Validated CAD and print files will be added after the revised housing has been
physically assembled and checked.

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
