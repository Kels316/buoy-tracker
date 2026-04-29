# Buoy Tracker — Build & Flash Guide

Custom Meshtastic firmware for a GPS buoy tracker. A tracker node broadcasts position every 30 seconds on a private encrypted channel. The T-Deck receiver displays live position, bearing, speed, and range.

Two tracker node options are supported: the **LilyGo T-Beam** (original) and the **Heltec WiFi LoRa 32 V4** (newer, smaller form factor with dedicated GNSS connector).

---

## Repo structure

```
buoy-tracker/
├── nodes/
│   ├── tbeam/                     ← T-Beam tracker node
│   │   ├── tracker_channel.h      ← PSK + channel + GPS pin config
│   │   └── src/modules/           ← TrackerModule, StrobeModule
│   └── heltec-v4/                 ← Heltec V4 tracker node
│       ├── tracker_channel.h      ← PSK + channel + GPS pin config
│       └── src/modules/           ← TrackerModule, StrobeModule
├── receiver/tdeck/src/modules/    ← TrackerDisplayModule (T-Deck UI pages)
├── docs/                          ← wiring diagrams, main.cpp patch files
└── tools/                         ← download_tiles.py (offline map prep)
```

---

## Hardware

### T-Beam tracker node

| Item | Detail |
|---|---|
| Board | LilyGo T-Beam v1.1 (ESP32) |
| GPS | External M8Q-5883 module — connected to free UART pins |
| Strobe | 5V white LED via MOSFET on GPIO 13 |
| LoRa region | ANZ |

#### T-Beam — GPS wiring (M8Q-5883 module)

| M8Q pin | T-Beam GPIO | Direction | Purpose |
|---|---|---|---|
| TX | GPIO 36 | GPS → Board | Serial data into T-Beam |
| RX | GPIO 25 | Board → GPS | Serial data from T-Beam |
| VCC | 3.3V | — | Power |
| GND | GND | — | Ground |

#### T-Beam — recovery strobe circuit (GPIO 13)

```
T-Beam 5V ── LED(+) ── LED(–) ──[56Ω]── 2N7000 Drain
2N7000 Source ── GND
GPIO 13 ──[100Ω]── 2N7000 Gate
2N7000 Gate ──[10kΩ]── GND
```

The 10kΩ gate pulldown prevents the strobe firing at boot when the GPIO floats.

---

### Heltec WiFi LoRa 32 V4 tracker node

| Item | Detail |
|---|---|
| Board | Heltec WiFi LoRa 32 V4 (ESP32-S3R2, SX1262) |
| GPS | External M8Q module via dedicated SH1.25-8P GNSS connector |
| Strobe | 5V white LED via MOSFET on GPIO 5 |
| LoRa region | ANZ |

#### Heltec V4 — GPS wiring (SH1.25-8P GNSS connector)

The board has a dedicated 8-pin GNSS connector — connect the M8Q directly to it using the matching cable. The four signals used are:

| Connector pin | Signal name | GPIO | Direction | Purpose |
|---|---|---|---|---|
| 1 | GND | GND | — | Ground |
| 2 | 3V3 | 3.3V | — | Power |
| 5 | GNSS_RST | GPIO 42 | Board → GPS | Reset (hold low >100ms to reset) |
| 6 | GNSS_PPS | GPIO 41 | GPS → Board | 1 pulse-per-second (not used in firmware) |
| 7 | GNSS_TX | GPIO 39 | Board → GPS | Serial command data to GPS |
| 8 | GNSS_RX | GPIO 38 | GPS → Board | Serial position data into board |

> **Pin conflict warning:** GPIO 36 is `VEXT_ENABLE` (external power control, active low) and GPIO 13 is `LORA_BUSY` (SX1262 DIO2). Neither can be used for GPS or strobe.

#### Heltec V4 — compass wiring (QMC5883L module, optional)

The QMC5883L shares the OLED I2C bus. OLED is at address 0x3C and compass is at 0x0D — no conflict.

| Compass pin | Heltec V4 pin | GPIO | Note |
|---|---|---|---|
| VCC | 3.3V | — | 3.3V only — do not use 5V |
| GND | GND | — | Ground |
| SDA | Header J2 or J3 SDA | GPIO 17 | Shared OLED bus |
| SCL | Header J2 or J3 SCL | GPIO 18 | Shared OLED bus |

#### Heltec V4 — recovery strobe circuit (GPIO 5)

```
5V ── LED(+) ── LED(–) ──[56Ω]── 2N7000 Drain
2N7000 Source ── GND
GPIO 5 ──[100Ω]── 2N7000 Gate
2N7000 Gate ──[10kΩ]── GND
```

GPIO 5 is on Header J3, pin 16. The 10kΩ gate pulldown prevents the strobe firing at boot.

#### Heltec V4 — indicator LED wiring (optional, external)

| LED purpose | GPIO | Header J3 pin | Behaviour |
|---|---|---|---|
| Power on | GPIO 2 | Pin 13 | Always on when board is running |
| TX flash | GPIO 4 | Pin 15 | 200 ms flash per position packet sent |

---

### T-Deck receiver

| Item | Detail |
|---|---|
| Board | LilyGo T-Deck Plus (ESP32-S3) |
| Display | Stock MUI with two extra pages (radar + data) |

---

## Prerequisites (one-time setup)

### 1. Install PlatformIO with Python 3.12

Python 3.14 has a known conflict with PlatformIO's config parser. Use 3.12:

```bash
brew install python@3.12
pipx install platformio --python /opt/homebrew/opt/python@3.12/bin/python3.12
```

PlatformIO CLI will be at `~/.local/bin/pio`.

### 2. Clone Meshtastic firmware 2.7.15

```bash
cd ~/Documents
git clone https://github.com/meshtastic/firmware.git firmware-2.7.15
cd firmware-2.7.15
git checkout v2.7.15.567b8ea
git submodule update --init
```

### 3. Clone this repo alongside firmware

```bash
git clone https://github.com/Kels316/buoy-tracker.git ~/Documents/buoy-tracker
```

The firmware repo expects `buoy-tracker/` to be a sibling of `firmware-2.7.15/` — both inside `~/Documents/`.

### 4. Add the tracker envs to platformio.ini

Append the following to `~/Documents/firmware-2.7.15/platformio.ini`. Both the T-Beam and Heltec V4 envs are included; build whichever node you need.

```ini
[env:tbeam-tracker]
extends = esp32_base
board = ttgo-tbeam
board_check = false
lib_ldf_mode = chain
upload_speed = 921600
build_type = release
build_src_filter =
    ${esp32_base.build_src_filter}
    +<../../buoy-tracker/nodes/tbeam/src/**>
build_flags =
    ${esp32_base.build_flags}
    -D TBEAM_V10
    -I variants/esp32/tbeam
    -I../buoy-tracker/nodes/tbeam
    -I../buoy-tracker/nodes/tbeam/src
    -Isrc/platform/esp32
    -DHAS_SCREEN=0
    -DUSE_SH1106=0
    -DUSE_SSD1306=0
    -DUSE_TRACKER_MODULE=1
    -DGPS_POWER_TOGGLE=0
    -DPOSITION_BROADCAST_SECS=3600
    -DMESHTASTIC_EXCLUDE_WEBSERVER=1

[heltec_v4_base]
extends = esp32s3_base
board = heltec_v4
board_check = true
board_build.partitions = default_16MB.csv
build_flags =
    ${esp32s3_base.build_flags}
    -D HELTEC_V4
    -I variants/esp32s3/heltec_v4
lib_deps =
    ${esp32s3_base.lib_deps}

[env:heltec-v4-tracker]
extends = heltec_v4_base
upload_speed = 921600
build_type = release
build_src_filter =
    ${heltec_v4_base.build_src_filter}
    +<../../buoy-tracker/nodes/heltec-v4/src/**>
build_flags =
    ${heltec_v4_base.build_flags}
    -D HELTEC_V4_OLED
    -D USE_SSD1306
    -D LED_PIN=35
    -D RESET_OLED=21
    -D I2C_SDA=17
    -D I2C_SCL=18
    -D I2C_SDA1=4
    -D I2C_SCL1=3
    -I../buoy-tracker/nodes/heltec-v4
    -I../buoy-tracker/nodes/heltec-v4/src
    -D HAS_SCREEN=0
    -D USE_TRACKER_MODULE=1
    -D GPS_POWER_TOGGLE=0
    -D POSITION_BROADCAST_SECS=3600
    -D MESHTASTIC_EXCLUDE_WEBSERVER=1
```

### 5. Patch main.cpp

The patch is different for each tracker node. Reference files are in `docs/`.

**T-Beam** — see `docs/tbeam-main-patch.cpp`

In `~/Documents/firmware-2.7.15/src/main.cpp`, add near the top:
```cpp
#include "modules/TrackerModule.h"
#include "tracker_channel.h"
```

Inside `void setup()`, after `nodeDB->init()` and before `service->init()`:
```cpp
setupTrackerChannel();
trackerModule = new TrackerModule();
strobeModule  = new StrobeModule();
```

**Heltec V4** — see `docs/heltec-v4-main-patch.cpp`

Same two edits, same locations, same code — the include paths and pin constants are resolved by the build system from the correct `nodes/heltec-v4/` folder.

---

## Building and flashing

### T-Beam

```bash
cd ~/Documents/firmware-2.7.15
~/.local/bin/pio run -e tbeam-tracker --target upload --upload-port /dev/cu.usbserial-XXXXXXXX
```

### Heltec V4

```bash
cd ~/Documents/firmware-2.7.15
~/.local/bin/pio run -e heltec-v4-tracker --target upload --upload-port /dev/cu.usbmodem-XXXXXXXX
```

Replace the port with your actual device port from `ls /dev/cu.*`.

If the build hangs, delete the SCons state file and retry:
```bash
rm -f .pio/build/<env>/.sconsign314.dblite
```

### T-Deck (receiver)

```bash
cd ~/Documents/firmware
~/.local/bin/pio run -e t-deck-tracker-display --target upload --upload-port /dev/cu.usbmodem-XXXXXXXX
```

---

## Channel configuration

All nodes broadcast on channel **slot 1** named `TRACKER` with AES256 encryption. Channel slot 0 must remain the default primary.

PSK (bytes):
```
0x4a,0x3f,0x8c,0x21,0xd7,0x55,0xb2,0x09,0xe1,0x7a,0x44,0xfc,0x30,0x8e,0x6b,0xd3,
0x92,0x1c,0x5f,0xa8,0x77,0x03,0xe6,0x4d,0xbb,0x29,0x10,0x58,0xc4,0x9d,0x6e,0xf1
```

PSK (base64): `Sj+MIddVsgnhekT8MI5r05IcX6h3A+ZNuykQWMSdbvE=`

To configure a node via CLI (both tracker nodes and the T-Deck must match):
```bash
meshtastic --port /dev/cu.<port> --ch-index 1 --ch-set name TRACKER --ch-set psk base64:Sj+MIddVsgnhekT8MI5r05IcX6h3A+ZNuykQWMSdbvE=
```

**Note:** Do not use `--ch-set-enabled true` — it breaks channel config on these devices.

---

## Data screen — speed and COG filtering

Speed (knots) and course over ground (COG) are calculated using an anchor model:

- The first position packet received is stored as the anchor (reference point).
- All subsequent packets compare current position against the anchor.
- If displacement from the anchor is less than **15 metres**, speed and COG are reported as **0 kt / 0°** to filter out GPS jitter.
- If displacement exceeds 15 metres, speed and COG are calculated from the anchor to the current position. This gives the net set and drift over the deployment period, which is the relevant quantity for SAR operations.

This means: `0 kt / 0°` = stationary (within GPS noise); `--- / ---` = no data received yet.

---

## Updating

```bash
cd ~/Documents/buoy-tracker && git pull
```

Then rebuild and reflash. No file copying needed — the build pulls directly from `buoy-tracker/`.

---

## Troubleshooting

| Symptom | Fix |
|---|---|
| Build hangs at dependency scan | Delete `.pio/build/<env>/.sconsign314.dblite` and retry |
| Port busy | `lsof /dev/cu.<port>` to find the holding process, then `kill <pid>` |
| Upload fails | Hold BOOT button on device while upload starts |
| No GPS lock | Normal indoors — take outside, cold fix takes 2–5 min |
| T-Deck shows zero position | Confirm TRACKER channel is slot 1 on both nodes |
| Packets not received | Confirm both nodes use identical PSK |
| Strobe always on at boot | Check 10kΩ gate pulldown on MOSFET circuit |
| Heltec V4 strobe not working | Verify strobe is on GPIO 5 — GPIO 13 is LORA_BUSY and cannot be used |
| Heltec V4 GPS not locking | Check GNSS connector orientation; GNSS_RX=GPIO38, GNSS_TX=GPIO39 |
| Battery shows `--` on T-Deck | Normal for first 5 min — telemetry interval is 300 s |
