# Meshtastic Tracker Display — T-Deck Module

Adds two extra screen pages to the LilyGo T-Deck showing live data
from the tbeam-tracker GPS node over the private TRACKER channel.

All standard T-Deck Meshtastic functionality is preserved — these
are additional pages in the existing screen cycle, not a replacement.

---

## What it adds

| Page | Content |
|---|---|
| Map view | Position dot with heading arrow, coordinates, time since last packet |
| Data view | Lat, lon, altitude, heading, satellites, PDOP, RSSI, time since last packet |

Navigate to these pages using the T-Deck trackball click or keyboard,
the same way you navigate any other Meshtastic screen.

---

## Requirements

- LilyGo T-Deck running Meshtastic firmware 2.7.x
- The tbeam-tracker node transmitting on channel 0 with your private PSK
- Both devices must use identical channel name (`TRACKER`) and PSK

---

## Step 1 — Clone the Meshtastic firmware

```bash
cd ~/Documents
git clone https://github.com/meshtastic/firmware.git
cd firmware
git submodule update --init
```

---

## Step 2 — Clone this repo

```bash
git clone https://github.com/Kels316/tbeam-tracker.git ~/Documents/tbeam-tracker
```

---

## Step 3 — Copy custom files into firmware

```bash
cd ~/Documents/firmware

cp ~/Documents/tbeam-tracker/tdeck-display/src/modules/TrackerDisplayModule.h   src/modules/
cp ~/Documents/tbeam-tracker/tdeck-display/src/modules/TrackerDisplayModule.cpp src/modules/
cp ~/Documents/tbeam-tracker/tdeck-display/platformio.ini .
```

---

## Step 4 — Patch main.cpp

```bash
open -e src/main.cpp
```

Add near the top with the other includes:

```cpp
#include "modules/TrackerDisplayModule.h"
```

Add inside `void setup()` after `nodeDB.init()`:

```cpp
trackerDisplayModule = new TrackerDisplayModule();
```

Save and close.

---

## Step 5 — Set your PSK

The T-Deck needs the same channel config as the tracker node.
Connect the T-Deck via USB and run:

```bash
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

Then apply to the T-Deck:

```bash
meshtastic --ch-index 0 --ch-set name TRACKER --ch-set psk base64:<your-key> --ch-set-enabled true
```

> Use your actual key bytes if you changed the defaults in tracker_channel.h

---

## Step 6 — Build and flash

```bash
cd ~/Documents/firmware
pio run -e t-deck-tracker-display --target upload
```

---

## Step 7 — Verify

Open the serial monitor:

```bash
pio device monitor --baud 115200
```

When a tracker packet arrives you should see:

```
TrackerDisplayModule: position updated from node 0x???????? lat=... lon=... track=...
```

The two new pages will then appear in the T-Deck screen cycle.

---

## Troubleshooting

| Symptom | Fix |
|---|---|
| Pages don't appear | Check main.cpp edits match main_patch.cpp exactly |
| "No data yet" on screen | Verify both devices use identical PSK and channel name |
| Build fails | Ensure both .h and .cpp are in `src/modules/` |
| Old pages still show | Do a full erase flash: `pio run -e t-deck-tracker-display --target erase` then upload |
