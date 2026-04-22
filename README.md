# Buoy Tracker — Build & Flash Guide

Custom Meshtastic firmware for a GPS buoy tracker. The T-Beam node broadcasts position every 30 seconds on a private encrypted channel. The T-Deck receiver displays live position, bearing, speed, and range.

---

## Repo structure

```
buoy-tracker/
├── nodes/tbeam/src/modules/   ← TrackerModule, StrobeModule
├── nodes/tbeam/               ← tracker_channel.h (PSK + channel setup)
├── receiver/tdeck/src/modules/ ← TrackerDisplayModule (T-Deck UI pages)
└── docs/                      ← wiring diagrams, circuit references
```

---

## Hardware

### T-Beam tracker node

| Item | Detail |
|---|---|
| Board | LilyGo T-Beam v1.1 (ESP32) |
| GPS | External M8Q-5883 module — RX=GPIO 36, TX=GPIO 25 |
| Strobe | 5V white LED via MOSFET on GPIO 13 |
| LoRa region | ANZ |

### T-Beam wiring — M8Q-5883 GPS module

| Module pin | T-Beam pin | Purpose |
|---|---|---|
| TX | GPIO 36 | GPS UART into T-Beam |
| RX | GPIO 25 | GPS UART from T-Beam |
| VCC | 3.3V | Power |
| GND | GND | Ground |

### Recovery strobe circuit (GPIO 13)

```
T-Beam 5V ── LED(+) ── LED(–) ──[56Ω]── 2N7000 Drain
2N7000 Source ── GND
GPIO 13 ──[100Ω]── 2N7000 Gate
2N7000 Gate ──[10kΩ]── GND
```

The 10kΩ gate pulldown prevents the strobe firing at boot when the GPIO floats.

### T-Deck receiver

| Item | Detail |
|---|---|
| Board | LilyGo T-Deck (ESP32-S3) |
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

### 3. Clone this repo

```bash
git clone https://github.com/Kels316/buoy-tracker.git ~/Documents/buoy-tracker
```

### 4. Add the tbeam-tracker env to platformio.ini

Append this block to `~/Documents/firmware-2.7.15/platformio.ini`:

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
    +<../buoy-tracker/nodes/tbeam/src/**>
build_flags =
    ${esp32_base.build_flags}
    -D TBEAM_V10
    -I variants/esp32/tbeam
    -I../buoy-tracker/nodes/tbeam
    -Isrc/platform/esp32
    -DHAS_SCREEN=0
    -DUSE_SH1106=0
    -DUSE_SSD1306=0
    -DUSE_TRACKER_MODULE=1
    -DGPS_POWER_TOGGLE=0
    -DPOSITION_BROADCAST_SECS=3600
    -DMESHTASTIC_EXCLUDE_WEBSERVER=1
```

No manual file copying needed — the build pulls directly from `buoy-tracker/`.

### 5. Patch main.cpp (T-Beam)

In `~/Documents/firmware-2.7.15/src/main.cpp`, add near the top includes:

```cpp
#include "modules/TrackerModule.h"
#include "modules/StrobeModule.h"
#include "tracker_channel.h"
```

Inside `void setup()`, after `nodeDB->init()`:

```cpp
setupTrackerChannel();
trackerModule = new TrackerModule();
strobeModule  = new StrobeModule();
```

---

## Building and flashing

### T-Beam

```bash
cd ~/Documents/firmware-2.7.15
~/.local/bin/pio run -e tbeam-tracker --target upload --upload-port /dev/cu.usbserial-XXXXXXXX
```

Replace the port with your actual device port from `ls /dev/cu.*`.

If the build hangs, delete the SCons state file and retry:

```bash
rm -f .pio/build/tbeam-tracker/.sconsign314.dblite
```

### T-Deck

```bash
cd ~/Documents/firmware
~/.local/bin/pio run -e t-deck-tracker-display --target upload --upload-port /dev/cu.usbmodem-XXXXXXXX
```

---

## Channel configuration

The tracker broadcasts on channel **slot 1** named `TRACKER` with AES256 encryption.

PSK (bytes):
```
0x4a,0x3f,0x8c,0x21,0xd7,0x55,0xb2,0x09,0xe1,0x7a,0x44,0xfc,0x30,0x8e,0x6b,0xd3,
0x92,0x1c,0x5f,0xa8,0x77,0x03,0xe6,0x4d,0xbb,0x29,0x10,0x58,0xc4,0x9d,0x6e,0xf1
```

PSK (base64): `Sj+MIddVsgnhekT8MI5r05IcX6h3A+ZNuykQWMSdbvE=`

To configure via CLI (both nodes must match):

```bash
meshtastic --ch-index 1 --ch-set name TRACKER --ch-set psk base64:Sj+MIddVsgnhekT8MI5r05IcX6h3A+ZNuykQWMSdbvE=
```

**Note:** Do not use `--ch-set-enabled true` — it breaks channel config on these devices.

---

## Updating

```bash
cd ~/Documents/buoy-tracker && git pull
```

Then rebuild and reflash. No file copying needed.

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
