#pragma once
#include "MeshModule.h"
#include "SinglePortModule.h"
#include "mesh/generated/meshtastic/mesh.pb.h"

// 20 fixes = 10 minutes at 30s interval
static constexpr uint8_t HISTORY_SIZE    = 20;
static constexpr float   MIN_MOVE_METRES = 3.0f;

struct PositionFix {
    int32_t  latitude_i  = 0;
    int32_t  longitude_i = 0;
    uint32_t rx_ms       = 0;
    bool     valid       = false;
};

struct TrackerData {
    int32_t  latitude_i   = 0;
    int32_t  longitude_i  = 0;
    int32_t  altitude     = 0;
    uint32_t ground_track = 0;
    uint32_t sats_in_view = 0;
    uint32_t PDOP         = 0;
    uint32_t time         = 0;
    uint32_t last_rx_ms   = 0;
    int8_t   rssi         = 0;
    uint32_t node_id      = 0;
    bool     valid        = false;
    float    speed_kn     = 0.0f;
    float    cog_deg      = 0.0f;
    uint8_t  window_fills = 0;
    bool     motion_valid = false;
};

// Shared state — defined in TrackerDisplayModule.cpp
extern TrackerData g_tracker;
extern PositionFix g_history[HISTORY_SIZE];
extern uint8_t     g_histHead;
extern uint8_t     g_histCount;

// ── Radar page + packet handler ───────────────────────────────
class TrackerRadarModule : public SinglePortModule
{
  public:
    TrackerRadarModule();
    bool wantUIFrame() override { return true; }
    void drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state,
                   int16_t x, int16_t y) override;

  protected:
    bool wantPacket(const meshtastic_MeshPacket *p) override {
        LOG_INFO("TrackerRadarModule: wantPacket portnum=%d\n", p->decoded.portnum);
        return p->decoded.portnum == meshtastic_PortNum_POSITION_APP;
    }
    ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;

  private:
    void   updateHistory(int32_t lat_i, int32_t lon_i, uint32_t rx_ms);
    void   drawCompassRose(OLEDDisplay *display, int16_t cx, int16_t cy,
                           int16_t r, float rotDeg);
    void   drawBoatIcon(OLEDDisplay *display, int16_t cx, int16_t cy, int16_t sz);
    void   drawArrow(OLEDDisplay *display, int16_t cx, int16_t cy,
                     int16_t r, float angleDeg);
    float  bearingTo(float lat1, float lon1, float lat2, float lon2);
    float  distanceMetres(float lat1, float lon1, float lat2, float lon2);
    String formatDistance(float metres);
    String formatAge(uint32_t now_ms);
};

// ── Data page ─────────────────────────────────────────────────
class TrackerDataModule : public MeshModule
{
  public:
    TrackerDataModule();
    bool wantUIFrame() override { return true; }
    void drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state,
                   int16_t x, int16_t y) override;

  protected:
    bool wantPacket(const meshtastic_MeshPacket *p) override { return false; }
    ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override
        { return ProcessMessage::CONTINUE; }

  private:
    String formatAge(uint32_t now_ms);
};

extern TrackerRadarModule *trackerRadarModule;
extern TrackerDataModule  *trackerDataModule;
