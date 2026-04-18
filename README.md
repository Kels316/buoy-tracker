# Meshtastic GPS Tracker — Build & Flash Guide
## Target: TTGO T-Beam (ESP32)

---

## What this firmware does

| Feature | Detail |
|---|---|
| GPS broadcast | Every **30 seconds** via LoRa |
| Compass heading | QMC5883L via I2C, packed into `ground_track` field |
| Channel | Private, PSK baked into firmware |
| Display | **Disabled** (no OLED driver loaded) |
| Bluetooth | Disabled (saves ~60 KB flash) |
| Power LED | GPIO 2 — on solid when running |
| TX LED | GPIO 4 — flashes 200 ms on each packet |
| Recovery strobe | GPIO 13 — 3 blinks/30s (lock), 3 blinks/5s (no lock) |

---

## 1. Prerequisites

```bash
# Python 3.8+ required
pip install platformio

# Clone the upstream Meshtastic firmware
git clone https://github.com/meshtastic/firmware.git
cd firmware
git checkout master   # or pin to a release tag, e.g. v2.5.x
```

---

## 2. Copy custom files

Copy this project's files into the cloned firmware repo:

```bash
# From inside the firmware/ directory:
cp /path/to/meshtastic-tracker/src/modules/TrackerModule.h  src/modules/
cp /path/to/meshtastic-tracker/src/modules/TrackerModule.cpp src/modules/
cp /path/to/meshtastic-tracker/src/tracker_channel.h        src/
```

---

## 3. Patch main.cpp

Open `src/main.cpp` in your editor.

### 3a. Add includes (near the top, with other module includes):
```cpp
#include "modules/TrackerModule.h"
#include "tracker_channel.h"
```

### 3b. Add setup calls inside `void setup()`, after `nodeDB.init()` and before `service.init()`:
```cpp
// --- Tracker customisation ---
setupTrackerChannel();
trackerModule = new TrackerModule();
// --- end tracker customisation ---
```

---

## 4. Set your private PSK

Open `src/tracker_channel.h` and replace `TRACKER_PSK` with your own 32-byte key:

```bash
# Generate a random key
openssl rand -hex 32
```

Convert the output to `0x??` byte notation and paste into the array.
**Every node that needs to receive these packets must be flashed with the same key.**

---

## 5. Build

Copy `platformio.ini` from this project into the firmware root (it extends the upstream tbeam env):

```bash
cp /path/to/meshtastic-tracker/platformio.ini .

# Build
pio run -e tbeam-tracker
```

Build output will be in `.pio/build/tbeam-tracker/firmware.bin`.

---

## 6. Flash

Connect your T-Beam via USB, then:

```bash
pio run -e tbeam-tracker --target upload
```

Or use the Meshtastic web flasher / esptool manually:

```bash
esptool.py --chip esp32 --port /dev/ttyUSB0 \
  write_flash 0x10000 .pio/build/tbeam-tracker/firmware.bin
```

---

## 7. Hardware — Wiring

### M8Q-5883 module connections

| Module pin | T-Beam pin | Purpose |
|---|---|---|
| TX | GPIO 12 | GPS UART RX into T-Beam |
| RX | GPIO 15 | GPS UART TX from T-Beam |
| SDA | GPIO 21 | QMC5883L I2C data |
| SCL | GPIO 22 | QMC5883L I2C clock |
| VCC | 3.3V | Power |
| GND | GND | Ground |

### LED wiring

> GPIO 21 is now used for I2C SDA — power LED moved to GPIO 2.

**Power LED (always on) — GPIO 2:**
```
GPIO 2 ──[220Ω]── LED anode (+)
                  LED cathode (–) ── GND
```

**TX flash LED — GPIO 4:**
```
GPIO 4 ──[220Ω]── LED anode (+)
                  LED cathode (–) ── GND
```

Use standard 3mm or 5mm LEDs. 220Ω is correct for 3.3V logic.

### Recovery strobe circuit (GPIO 13)

```
T-Beam 5V ── LED(+) ── LED(–) ──[56Ω]── 2N7000 Drain
2N7000 Source ─────────────────────────── GND
GPIO 13 ──[100Ω]───────────────────────── 2N7000 Gate
2N7000 Gate ──[10kΩ]───────────────────── GND
```

The 10kΩ gate pulldown is important — without it the GPIO floats at boot and the strobe may fire unexpectedly.

### Magnetic declination

Open `TrackerModule.h` and set `MAG_DECLINATION` for your deployment location.
Brisbane is pre-set to +11.5 degrees. Find your value at https://www.magnetic-declination.com

---

## 8. Receiving the data

Any standard Meshtastic node or app configured with the **same channel name + PSK** will decode the position packets. You can set this up via:

```bash
meshtastic --ch-set name TRACKER --ch-set psk <your-key-base64> --ch-index 0
```

Or import the channel URL generated from the Meshtastic app.

---

## 9. Receiver node setup

The receiver runs **stock Meshtastic firmware** — no custom build needed. It just needs the matching channel config.

### What must match exactly

| Setting | Value |
|---|---|
| Channel name | `TRACKER` |
| PSK | The 32 bytes set in `tracker_channel.h` |
| Channel slot | 0 (primary) |

### Step 1 — Convert your PSK to base64

Run this on your Mac, substituting your actual key bytes if you changed the defaults:

```python
python3 -c "
import base64
key = bytes([
    0x4a, 0x3f, 0x8c, 0x21, 0xd7, 0x55, 0xb2, 0x09,
    0xe1, 0x7a, 0x44, 0xfc, 0x30, 0x8e, 0x6b, 0xd3,
    0x92, 0x1c, 0x5f, 0xa8, 0x77, 0x03, 0xe6, 0x4d,
    0xbb, 0x29, 0x10, 0x58, 0xc4, 0x9d, 0x6e, 0xf1
])
print(base64.b64encode(key).decode())
"
```

### Step 2a — Apply via Meshtastic CLI (receiver connected by USB)

```bash
meshtastic --ch-index 0 --ch-set name TRACKER --ch-set psk base64:<your-base64-key> --ch-set-enabled true
```

### Step 2b — Apply via phone app

1. Open Meshtastic app → connect to receiver node
2. Channel Config → Channel 0
3. Set name to `TRACKER`
4. Set PSK to the same key
5. Save

### What you'll see

Position packets from the tracker appear as a node on the map in the Meshtastic app, including GPS coordinates, altitude, satellite count, and compass heading (`ground_track` field). Any node or app on the same channel decodes them automatically — no extra software needed.

> **Note:** If you changed the default PSK bytes in `tracker_channel.h` before building, use your updated bytes when generating the base64 string — not the placeholder defaults above.


---

## Troubleshooting

| Symptom | Fix |
|---|---|
| Power LED not on | Check wiring on GPIO 2; change `POWER_LED_PIN` in TrackerModule.h if needed |
| Compass heading missing | Verify SDA/SCL wiring; check serial monitor for 'QMC5883L not found' |
| TX LED not flashing | Check wiring on GPIO 4; verify `TX_LED_PIN` in TrackerModule.h |
| "no GPS lock" in logs | Give the T-Beam a clear sky view; cold fix can take 2–5 min |
| Packets not received | Confirm both nodes use identical PSK bytes |
| Build fails on TrackerModule | Ensure both .h and .cpp are in `src/modules/` |
| Strobe not flashing | Check MOSFET wiring on GPIO 13; verify gate pulldown resistor is in place |
| Strobe always on | Missing 10kΩ gate pulldown — GPIO floats high at boot without it |
