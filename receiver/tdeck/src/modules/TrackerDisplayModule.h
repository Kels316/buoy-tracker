#pragma once
#include "MeshModule.h"
#include "SinglePortModule.h"
#include "mesh/generated/meshtastic/mesh.pb.h"

static constexpr float MIN_MOVE_METRES = 15.0f; // below this from anchor = stationary

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
    uint32_t ground_track = 0;   // compass heading * 100
    uint32_t sats_in_view = 0;
    uint32_t PDOP         = 0;
    uint32_t time         = 0;
    uint32_t last_rx_ms   = 0;
    int8_t   rssi         = 0;
    uint32_t node_id      = 0;
    uint32_t battery_pct  = 0;     // 0-100, or 101 = USB/charging
    bool     valid        = false;
    float    speed_kn     = 0.0f;
    float    cog_deg      = 0.0f;
    uint32_t window_fills = 0;  // packet count since first fix
    bool     motion_valid = false;
};

// Shared state — defined in TrackerDisplayModule.cpp
extern TrackerData g_tracker;
extern PositionFix g_anchor;   // first fix ever received — never overwritten
extern bool        g_anchorSet;

// ── Packet handler ─────────────────────────────────────────────
// Receives TRACKER channel Position packets, updates g_tracker + history.
// TrackerScreens handles all display via LVGL timer.
class TrackerRadarModule : public SinglePortModule
{
  public:
    TrackerRadarModule();

  protected:
    bool wantPacket(const meshtastic_MeshPacket *p) override {
        return p->decoded.portnum == meshtastic_PortNum_POSITION_APP;
    }
    ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;

  private:
    void  updateHistory(int32_t lat_i, int32_t lon_i, uint32_t rx_ms);
    float bearingTo(float lat1, float lon1, float lat2, float lon2);
    float distanceMetres(float lat1, float lon1, float lat2, float lon2);
};

// ── LVGL screen manager ────────────────────────────────────────
// Creates two standalone LVGL screens (radar + data).
// Call TrackerScreens::init() from main.cpp after device-ui setup.
// Navigation:
//   Main screen  →  press 'T' on keyboard  →  radar screen
//   Radar screen →  trackball press         →  data screen
//   Data screen  →  trackball press         →  main screen
class TrackerScreens
{
  public:
    static void init();       // call once from main.cpp
    static void enterRadar();
    static void enterData();
    static void exitToMain();

    enum class Page { None, Radar, Data };
    static Page currentPage;  // read by indev wrapper lambda

    friend void trackerRunSetup(); // called from tft task loop in tftSetup.cpp

  private:
    static void buildScreens();
    static void redrawRadar();
    static void redrawData();

    // LVGL draw helpers (operate on an open lv_layer_t)
    static void canvasLine(void *layer, int x1, int y1, int x2, int y2,
                           uint32_t colHex, int w = 1);
    static void canvasCircle(void *layer, int cx, int cy, int r,
                             int strokeW, uint32_t colHex);
    static void canvasFilledCircle(void *layer, int cx, int cy, int r,
                                   uint32_t colHex);
    static void canvasText(void *layer, int x, int y, int w, int h,
                           const char *txt, uint32_t colHex);
    static void canvasTriangle(void *layer, int x0, int y0, int x1, int y1,
                               int x2, int y2, uint32_t colHex);

    // Radar drawing
    static void drawCompassRose(void *layer, int cx, int cy, int r, float rotDeg);
    static void drawBoatIcon(void *layer, int cx, int cy, int sz);
    static void drawArrow(void *layer, int cx, int cy, int r, float angleDeg);
    static void drawBuoyDot(void *layer, int cx, int cy, int r);

    // Utility
    static float  bearingTo(float lat1, float lon1, float lat2, float lon2);
    static float  distanceMetres(float lat1, float lon1, float lat2, float lon2);
    static String formatDistance(float metres);
    static String formatAge(uint32_t now_ms);

    // LVGL callbacks
    static void onTimer(void *timer);          // lv_timer_t*
    static void onCustomScreenClick(void *event); // trackball press on custom screens

    static bool      hookDone;
    static void     *mainScr;      // lv_obj_t* — main device-ui screen
    static void     *radarScr;     // lv_obj_t*
    static void     *dataScr;      // lv_obj_t*
    static void     *radarCanvas;  // lv_obj_t*
    static uint8_t  *radarBuf;     // PSRAM canvas buffer

    // Data screen label pointers (lv_obj_t*)
    static void *lblLat, *lblLon, *lblAlt, *lblSpd, *lblCog;
    static void *lblHdg, *lblSats, *lblPdop, *lblRssi, *lblWin, *lblAge;
    static void *radarDist, *radarBrg, *radarHdg, *radarSpd, *radarAge, *radarTitle;
};

extern TrackerRadarModule *trackerRadarModule;
