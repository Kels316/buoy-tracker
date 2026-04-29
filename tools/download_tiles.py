#!/usr/bin/env python3
"""
Download OSM tiles for the Noosa Heads SAR area.

Saves tiles as individual PNGs in the structure the T-Deck expects:
  maps/{zoom}/{x}/{y}.png

Copy the entire 'maps' folder to the root of the SD card when done.

Bounding box:
  West:  Doonan (~152.970E)
  East:  ~2km offshore of Sunshine Beach (~153.125E)
  North: north of Noosa Heads (~26.340S)
  South: south of Sunshine Beach (~26.460S)

Usage:
  pip3 install requests
  python3 download_tiles.py
"""

import math
import time
import os
import sys
import requests

# ── Bounding box ──────────────────────────────────────────────────────────────
WEST  = 152.970   # Doonan
EAST  = 153.125   # ~2km offshore of Sunshine Beach
NORTH = -26.340   # north of Noosa Heads
SOUTH = -26.460   # south of Sunshine Beach

# ── Zoom levels ───────────────────────────────────────────────────────────────
ZOOM_MIN = 10
ZOOM_MAX = 15     # z15 = ~20m resolution, safe for 32MB SD card

# ── Output directory ──────────────────────────────────────────────────────────
# Copy this entire folder to the root of the SD card
OUTPUT_DIR = "maps"

# ── Tile source ───────────────────────────────────────────────────────────────
TILE_URL   = "https://tile.openstreetmap.org/{z}/{x}/{y}.png"
USER_AGENT = "NoosaSARTileDownloader/1.0 (personal SAR use)"
DELAY_SEC  = 0.1   # polite delay between requests


def deg2tile(lat_deg, lon_deg, zoom):
    lat_r = math.radians(lat_deg)
    n = 2 ** zoom
    x = int((lon_deg + 180.0) / 360.0 * n)
    y = int((1.0 - math.asinh(math.tan(lat_r)) / math.pi) / 2.0 * n)
    return x, y


def tile_range(zoom):
    x_min, y_min = deg2tile(NORTH, WEST, zoom)  # NW corner
    x_max, y_max = deg2tile(SOUTH, EAST, zoom)  # SE corner
    return x_min, x_max, y_min, y_max


def count_tiles():
    total = 0
    for zoom in range(ZOOM_MIN, ZOOM_MAX + 1):
        x_min, x_max, y_min, y_max = tile_range(zoom)
        total += (x_max - x_min + 1) * (y_max - y_min + 1)
    return total


def download():
    session = requests.Session()
    session.headers["User-Agent"] = USER_AGENT

    total = count_tiles()
    done = 0
    skipped = 0
    failed = 0

    print(f"Total tiles: {total}")
    print(f"Output: ./{OUTPUT_DIR}/")

    for zoom in range(ZOOM_MIN, ZOOM_MAX + 1):
        x_min, x_max, y_min, y_max = tile_range(zoom)
        n = (x_max - x_min + 1) * (y_max - y_min + 1)
        print(f"\nZoom {zoom}: {n} tiles")

        for x in range(x_min, x_max + 1):
            for y in range(y_min, y_max + 1):
                path = os.path.join(OUTPUT_DIR, str(zoom), str(x), f"{y}.png")

                # Resume: skip already downloaded tiles
                if os.path.exists(path):
                    skipped += 1
                    continue

                os.makedirs(os.path.dirname(path), exist_ok=True)

                url = TILE_URL.format(z=zoom, x=x, y=y)
                try:
                    r = session.get(url, timeout=15)
                    r.raise_for_status()
                    with open(path, "wb") as f:
                        f.write(r.content)
                    done += 1
                    time.sleep(DELAY_SEC)
                except Exception as e:
                    print(f"  FAIL z={zoom} x={x} y={y}: {e}")
                    failed += 1

                pct = int((done + skipped + failed) / total * 100)
                print(f"  {pct}%  ({done} downloaded, {skipped} cached, {failed} failed)\r",
                      end="", flush=True)

    print(f"\n\nDone. {done} downloaded, {skipped} already cached, {failed} failed.")
    print(f"\nCopy the '{OUTPUT_DIR}' folder to the root of the SD card.")


if __name__ == "__main__":
    try:
        import requests  # noqa: F401
    except ImportError:
        print("Missing dependency. Run:  pip3 install requests")
        sys.exit(1)

    download()