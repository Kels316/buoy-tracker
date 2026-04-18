#pragma once
#include "SinglePortModule.h"
#include "mesh/generated/meshtastic/mesh.pb.h"

/**
 * TrackerDisplayModule
 *
 * Meshtastic screen module for the LilyGo T-Deck.
 * Registers two extra pages in the existing screen cycle:
 *
 *   Page 1 — Map view   : position dot + heading arrow
 *   Page 2 — Data view  : lat, lon, alt, heading, sats, PDOP, RSSI, age
 *
 * Listens on the TRACKER private channel (channel index 0).
 * All other T-Deck screens and functions are unaffected.
 *
 * Target: Meshtastic firmware 2.7.x
 * Hardware: LilyGo T-Deck (ESP32-S3, ILI9341 320x240, SX1262)
 */

// ── Tracker data ──────────────────────────────────────────────
struct TrackerData {
    int32_t  latitude_i   = 0;   // degrees * 1e7
    int32_t  longitude_i  = 0;   // degrees * 1e7
    int32_t  altitude     = 0;   // metres
    uint32_t ground_track = 0;   // degrees * 100
    uint32_t sats_in_view = 0;
    uint32_t PDOP         = 0;
    uint32_t time         = 0;   // unix timestamp of GPS fix
    uint32_t last_rx_ms   = 0;   // millis() when last packet received
    int8_t   rssi         = 0;
    uint32_t node_id      = 0;
    bool     valid        = false;
};

class TrackerDisplayModule : public SinglePortModule
{
  public:
    TrackerDisplayModule();

    // Meshtastic screen integration
    bool wantUIFrame() override { return true; }
    void drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state,
                   int16_t x, int16_t y) override;
    int getNumExtraFrames() override { return 2; }

  protected:
    ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;

  private:
    void drawMapPage(OLEDDisplay *display, OLEDDisplayUiState *state,
                     int16_t x, int16_t y);
    void drawDataPage(OLEDDisplay *display, OLEDDisplayUiState *state,
                      int16_t x, int16_t y);
    void drawArrow(OLEDDisplay *display, int16_t cx, int16_t cy,
                   int16_t r, float angleDeg);
    String formatAge(uint32_t now_ms);

    TrackerData tracker;
    int         currentPage = 0;
};

extern TrackerDisplayModule *trackerDisplayModule;
