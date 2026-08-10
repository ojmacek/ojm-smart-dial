# Smart Dial CAN protocol

The current proof-of-concept uses Classical CAN with 11-bit identifiers at
500 kbit/s. The dial starts CAN communication after the startup animation has
finished.

## Dial state — `0x201`

The dial transmits this eight-byte frame every 200 ms.

| Byte | Signal | Encoding |
| --- | --- | --- |
| 0 | Flags | bit 0: power, bit 1: A/C |
| 1 | Temperature | value × 0.5 °C |
| 2 | Fan | `FF`: AUTO, `00`: OFF, `01`–`08`: manual level |
| 3 | Airflow | `00`: AUTO, `01`: FACE, `02`: FEET, `03`: SCREEN |
| 4 | Seat heating | `00`–`03` |
| 5 | Visible page | `00`: temperature, `01`: fan, `02`: airflow, `03`: seat |
| 6 | Alive counter | `00`–`0F`, incremented after each queued frame |
| 7 | Checksum | XOR of bytes 0–6 |

An initial state of power on, A/C on, 22.0 °C, automatic fan, automatic
airflow and no seat heating begins as:

```text
ID 201  DLC 8  DATA 03 2C FF 00 00 00 00 D0
```

## Vehicle command — `0x301`

Commands use two data bytes. Byte 0 selects the command and byte 1 contains
the requested value. The next periodic `0x201` frame confirms the applied
state.

| Command | Function | Value |
| --- | --- | --- |
| `01` | Set temperature | `20`–`3C` = 16.0–30.0 °C |
| `02` | Set fan | `FF`, or `00`–`08` |
| `03` | Set airflow | `00`–`03` |
| `04` | Set seat heating | `00`–`03` |
| `05` | Set A/C | `00`: off, non-zero: on |
| `06` | Set climate power | `00`: off, non-zero: on |

Examples for Cangaroo:

```text
ID 301  DLC 2  DATA 01 2D   # temperature 22.5 °C
ID 301  DLC 2  DATA 02 05   # manual fan level 5
ID 301  DLC 2  DATA 02 FF   # automatic fan
ID 301  DLC 2  DATA 03 02   # airflow to footwell
ID 301  DLC 2  DATA 04 03   # seat heating level 3
ID 301  DLC 2  DATA 05 00   # A/C off
ID 301  DLC 2  DATA 06 00   # climate interface off
```

All frames above are standard data frames. Extended ID, RTR and CAN FD must
remain disabled.
