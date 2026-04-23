#include "TrackerDisplayModule.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "configuration.h"
#include "graphics/Screen.h"
#include <Arduino.h>
#include <math.h>
#include <pb_decode.h>

// ── Global shared state ────────────────────────────────────────
TrackerData g_tracker;
PositionFix g_history[HISTORY_SIZE];
uint8_t     g_histHead  = 0;
uint8_t     g_histCount = 0;

TrackerRadarModule *trackerRadarModule;
TrackerDataModule  *trackerDataModule;

static constexpr float DEG2RAD   = M_PI / 180.0f;
static constexpr float RAD2DEG   = 180.0f / M_PI;
static constexpr float MPS_TO_KN = 1.94384f;

// ═══════════════════════════════════════════════════════════════
// TrackerRadarModule — packet handler + radar page
// ═══════════════════════════════════════════════════════════════

TrackerRadarModule::TrackerRadarModule()
    : SinglePortModule("TrackerRadar", meshtastic_PortNum_POSITION_APP)
{
    LOG_INFO("TrackerRadarModule: init\n");
}

// ── Packet handler ────────────────────────────────────────────
ProcessMessage TrackerRadarModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (mp.channel > 1) return ProcessMessage::CONTINUE;

    meshtastic_Position pos = meshtastic_Position_init_default;
    pb_istream_t stream = pb_istream_from_buffer(
        mp.decoded.payload.bytes, mp.decoded.payload.size);

    if (!pb_decode(&stream, &meshtastic_Position_msg, &pos)) {
        LOG_WARN("TrackerRadarModule: decode failed\n");
        return ProcessMessage::CONTINUE;
    }

    if (!pos.has_latitude_i || !pos.has_longitude_i) {
        LOG_INFO("TrackerRadarModule: pkt has no fix yet\n");
        return ProcessMessage::CONTINUE;
    }

    g_tracker.latitude_i   = pos.latitude_i;
    g_tracker.longitude_i  = pos.longitude_i;
    g_tracker.altitude     = pos.altitude;
    g_tracker.ground_track = pos.ground_track;
    g_tracker.sats_in_view = pos.sats_in_view;
    g_tracker.PDOP         = pos.PDOP;
    g_tracker.time         = pos.time;
    g_tracker.last_rx_ms   = millis();
    g_tracker.rssi         = mp.rx_rssi;
    g_tracker.node_id      = mp.from;
    g_tracker.valid        = true;

    updateHistory(pos.latitude_i, pos.longitude_i, millis());

    LOG_INFO("TrackerRadarModule: lat=%d lon=%d spd=%.2fkn cog=%.0f win=%u/20\n",
             pos.latitude_i, pos.longitude_i,
             g_tracker.speed_kn, g_tracker.cog_deg, g_tracker.window_fills);

    if (screen) screen->forceDisplay();
    return ProcessMessage::CONTINUE;
}

// ── 10-minute rolling history + SAR speed/COG ────────────────
void TrackerRadarModule::updateHistory(int32_t lat_i, int32_t lon_i,
                                        uint32_t rx_ms)
{
    g_history[g_histHead].latitude_i  = lat_i;
    g_history[g_histHead].longitude_i = lon_i;
    g_history[g_histHead].rx_ms       = rx_ms;
    g_history[g_histHead].valid       = true;
    g_histHead = (g_histHead + 1) % HISTORY_SIZE;
    if (g_histCount < HISTORY_SIZE) g_histCount++;

    g_tracker.window_fills = g_histCount;

    if (g_histCount < 2) {
        g_tracker.motion_valid = false;
        return;
    }

    uint8_t oldest = (g_histHead - g_histCount + HISTORY_SIZE) % HISTORY_SIZE;
    uint8_t newest = (g_histHead - 1            + HISTORY_SIZE) % HISTORY_SIZE;

    // COG: net drift bearing across the full window
    float latOld = g_history[oldest].latitude_i  / 1e7f;
    float lonOld = g_history[oldest].longitude_i / 1e7f;
    float latNew = g_history[newest].latitude_i  / 1e7f;
    float lonNew = g_history[newest].longitude_i / 1e7f;
    g_tracker.cog_deg = bearingTo(latOld, lonOld, latNew, lonNew);

    // Speed: total valid distance / total valid time (drift filter applied)
    float totalDistM = 0.0f, totalTimeSec = 0.0f;
    for (uint8_t i = 0; i < g_histCount - 1; i++) {
        uint8_t idxA = (oldest + i)     % HISTORY_SIZE;
        uint8_t idxB = (oldest + i + 1) % HISTORY_SIZE;
        if (!g_history[idxA].valid || !g_history[idxB].valid) continue;

        float latA = g_history[idxA].latitude_i  / 1e7f;
        float lonA = g_history[idxA].longitude_i / 1e7f;
        float latB = g_history[idxB].latitude_i  / 1e7f;
        float lonB = g_history[idxB].longitude_i / 1e7f;

        float distM = distanceMetres(latA, lonA, latB, lonB);
        float dtSec = (float)(g_history[idxB].rx_ms - g_history[idxA].rx_ms) / 1000.0f;

        if (distM < MIN_MOVE_METRES || dtSec < 1.0f) continue;
        totalDistM   += distM;
        totalTimeSec += dtSec;
    }

    g_tracker.speed_kn     = (totalTimeSec > 0.0f)
                               ? (totalDistM / totalTimeSec) * MPS_TO_KN
                               : 0.0f;
    g_tracker.motion_valid = true;
}

// ── Radar frame (scale-aware, targets 320×240 T-Deck TFT) ─────
void TrackerRadarModule::drawFrame(OLEDDisplay *display,
                                    OLEDDisplayUiState *state,
                                    int16_t x, int16_t y)
{
    const int16_t W = display->getWidth();
    const int16_t H = display->getHeight();

    // Compass rose — sized to fill the left portion of the display
    const int16_t rOuter = (H * 9) / 22;        // ~98px on 240px, ~26px on 64px
    const int16_t rInner = rOuter - (rOuter / 8);
    const int16_t cx     = x + rOuter + (W / 40);
    const int16_t cy     = y + H / 2;
    const int16_t infX   = cx + rOuter + (W / 20); // info panel left edge

    // Own position
    bool  ownFix = false;
    float ownLat = 0.0f, ownLon = 0.0f, ownHeading = 0.0f;
    if (localPosition.latitude_i != 0 || localPosition.longitude_i != 0) {
        ownLat     = localPosition.latitude_i   / 1e7f;
        ownLon     = localPosition.longitude_i  / 1e7f;
        ownHeading = localPosition.ground_track / 100.0f;
        ownFix     = true;
    }

    // Title
    display->setFont(ArialMT_Plain_10);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->drawString(x + 2, y + 2, "TRACKER");

    // Own heading — top right
    if (ownFix) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%03.0f\xC2\xB0M", ownHeading);
        display->setTextAlignment(TEXT_ALIGN_RIGHT);
        display->drawString(x + W - 2, y + 2, buf);
    }

    float rotation = ownFix ? ownHeading : 0.0f;
    drawCompassRose(display, cx, cy, rOuter, rotation);
    drawBoatIcon(display, cx, cy, rOuter / 10);

    if (g_tracker.valid && ownFix) {
        float buoyLat = g_tracker.latitude_i  / 1e7f;
        float buoyLon = g_tracker.longitude_i / 1e7f;

        float distM   = distanceMetres(ownLat, ownLon, buoyLat, buoyLon);
        float bearDeg = bearingTo(ownLat, ownLon, buoyLat, buoyLon);
        float relBear = fmodf(bearDeg - rotation + 360.0f, 360.0f);

        // Buoy dot on rose
        float   plotRad = (relBear - 90.0f) * DEG2RAD;
        int16_t bx = cx + (int16_t)(rInner * cosf(plotRad));
        int16_t by = cy + (int16_t)(rInner * sinf(plotRad));
        int16_t dotR = rOuter / 20 + 1;   // ~5px on 320×240
        display->fillCircle(bx, by, dotR);
        display->drawLine(cx, cy, bx, by);

        // Heading arrow on buoy dot
        float arrowLen = (float)(rOuter / 10);
        float buoyRelHead = fmodf((g_tracker.ground_track / 100.0f) - rotation + 360.0f, 360.0f);
        drawArrow(display, bx, by, (int16_t)arrowLen, buoyRelHead);

        // Info panel
        char buf[32];
        const int16_t rowH = H / 7;
        int16_t ry = y + (H / 9);

        display->setTextAlignment(TEXT_ALIGN_LEFT);

        // Distance — larger font for the most important number
        display->setFont(ArialMT_Plain_16);
        display->drawString(infX, ry, formatDistance(distM));
        ry += rowH;

        display->setFont(ArialMT_Plain_10);

        // True bearing to buoy
        snprintf(buf, sizeof(buf), "Brg %03.0f\xC2\xB0T", bearDeg);
        display->drawString(infX, ry, buf); ry += rowH;

        // Buoy compass heading (from ground_track field = compass hdg * 100)
        snprintf(buf, sizeof(buf), "Hdg %03.0f\xC2\xB0M", g_tracker.ground_track / 100.0f);
        display->drawString(infX, ry, buf); ry += rowH;

        // Speed over 10-min window
        if (g_tracker.motion_valid)
            snprintf(buf, sizeof(buf), "Spd %.1fkn", g_tracker.speed_kn);
        else
            snprintf(buf, sizeof(buf), "Spd ---");
        display->drawString(infX, ry, buf); ry += rowH;

        // Age of last fix
        display->drawString(infX, ry, formatAge(millis()));

    } else if (g_tracker.valid && !ownFix) {
        const int16_t rowH = H / 7;
        int16_t ry = y + (H / 9);
        char buf[24];
        display->setTextAlignment(TEXT_ALIGN_LEFT);
        display->setFont(ArialMT_Plain_10);
        display->drawString(infX, ry, "No own GPS"); ry += rowH;
        snprintf(buf, sizeof(buf), "%.5f", g_tracker.latitude_i  / 1e7f);
        display->drawString(infX, ry, buf); ry += rowH;
        snprintf(buf, sizeof(buf), "%.5f", g_tracker.longitude_i / 1e7f);
        display->drawString(infX, ry, buf); ry += rowH;
        display->drawString(infX, ry, formatAge(millis()));
    } else {
        display->setTextAlignment(TEXT_ALIGN_CENTER);
        display->setFont(ArialMT_Plain_16);
        display->drawString(x + W / 2, y + H / 2 - 8, "No buoy signal");
    }
}

// ── Compass rose ──────────────────────────────────────────────
void TrackerRadarModule::drawCompassRose(OLEDDisplay *display,
                                          int16_t cx, int16_t cy,
                                          int16_t r, float rotationDeg)
{
    display->drawCircle(cx, cy, r);
    display->drawCircle(cx, cy, r - (r / 15 + 1));  // inner ring

    const char *cardinals[] = { "N", "E", "S", "W" };
    int16_t labelR = r - r / 7;
    int16_t tickInner = r - r / 12;

    for (int i = 0; i < 4; i++) {
        float angleRad = ((float)(i * 90) - rotationDeg - 90.0f) * DEG2RAD;
        int16_t lx = cx + (int16_t)(labelR * cosf(angleRad));
        int16_t ly = cy + (int16_t)(labelR * sinf(angleRad));
        display->setTextAlignment(TEXT_ALIGN_CENTER);
        display->setFont(ArialMT_Plain_10);
        display->drawString(lx, ly - 5, cardinals[i]);
    }

    // 8 tick marks (45° intervals)
    for (int i = 0; i < 8; i++) {
        float angleRad = ((float)(i * 45) - rotationDeg - 90.0f) * DEG2RAD;
        int16_t x1 = cx + (int16_t)(tickInner * cosf(angleRad));
        int16_t y1 = cy + (int16_t)(tickInner * sinf(angleRad));
        int16_t x2 = cx + (int16_t)(r * cosf(angleRad));
        int16_t y2 = cy + (int16_t)(r * sinf(angleRad));
        display->drawLine(x1, y1, x2, y2);
    }
}

// ── Boat icon (own vessel, centre of rose) ────────────────────
void TrackerRadarModule::drawBoatIcon(OLEDDisplay *display,
                                       int16_t cx, int16_t cy, int16_t sz)
{
    if (sz < 3) sz = 3;
    // Simple triangle pointing up
    display->drawLine(cx,      cy - sz, cx - sz/2, cy + sz/2);
    display->drawLine(cx,      cy - sz, cx + sz/2, cy + sz/2);
    display->drawLine(cx - sz/2, cy + sz/2, cx + sz/2, cy + sz/2);
}

// ── Heading arrow on buoy dot ─────────────────────────────────
void TrackerRadarModule::drawArrow(OLEDDisplay *display,
                                    int16_t cx, int16_t cy,
                                    int16_t r, float angleDeg)
{
    if (r < 3) r = 3;
    float rad = (angleDeg - 90.0f) * DEG2RAD;
    int16_t tx = cx + (int16_t)(r * cosf(rad));
    int16_t ty = cy + (int16_t)(r * sinf(rad));
    int16_t bx = cx - (int16_t)((r * 0.6f) * cosf(rad));
    int16_t by = cy - (int16_t)((r * 0.6f) * sinf(rad));
    float w1r  = rad + M_PI * 0.75f;
    float w2r  = rad - M_PI * 0.75f;
    int16_t hw = (r / 3 < 2) ? 2 : r / 3;
    display->drawLine(bx, by, tx, ty);
    display->drawLine(tx, ty, tx + (int16_t)(hw * cosf(w1r)), ty + (int16_t)(hw * sinf(w1r)));
    display->drawLine(tx, ty, tx + (int16_t)(hw * cosf(w2r)), ty + (int16_t)(hw * sinf(w2r)));
}

// ── Haversine ─────────────────────────────────────────────────
float TrackerRadarModule::distanceMetres(float lat1, float lon1,
                                          float lat2, float lon2)
{
    const float R = 6371000.0f;
    float dLat = (lat2 - lat1) * DEG2RAD;
    float dLon = (lon2 - lon1) * DEG2RAD;
    float a = sinf(dLat/2)*sinf(dLat/2) +
              cosf(lat1*DEG2RAD)*cosf(lat2*DEG2RAD)*
              sinf(dLon/2)*sinf(dLon/2);
    return R * 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
}

// ── True bearing ──────────────────────────────────────────────
float TrackerRadarModule::bearingTo(float lat1, float lon1,
                                     float lat2, float lon2)
{
    float dLon = (lon2 - lon1) * DEG2RAD;
    float y = sinf(dLon) * cosf(lat2 * DEG2RAD);
    float x = cosf(lat1*DEG2RAD)*sinf(lat2*DEG2RAD) -
               sinf(lat1*DEG2RAD)*cosf(lat2*DEG2RAD)*cosf(dLon);
    return fmodf(atan2f(y, x) * RAD2DEG + 360.0f, 360.0f);
}

// ── Distance formatter ────────────────────────────────────────
String TrackerRadarModule::formatDistance(float metres)
{
    char buf[16];
    if (metres < 1852.0f)
        snprintf(buf, sizeof(buf), "%dm", (int)metres);
    else if (metres / 1852.0f < 10.0f)
        snprintf(buf, sizeof(buf), "%.2fnm", metres / 1852.0f);
    else
        snprintf(buf, sizeof(buf), "%.1fnm", metres / 1852.0f);
    return String(buf);
}

// ── Age formatter ─────────────────────────────────────────────
String TrackerRadarModule::formatAge(uint32_t now_ms)
{
    if (!g_tracker.valid || g_tracker.last_rx_ms == 0) return "--";
    uint32_t age_s = (now_ms - g_tracker.last_rx_ms) / 1000;
    char buf[12];
    if (age_s < 60)
        snprintf(buf, sizeof(buf), "%us ago", age_s);
    else if (age_s < 3600)
        snprintf(buf, sizeof(buf), "%um ago", age_s / 60);
    else
        snprintf(buf, sizeof(buf), "%uh ago", age_s / 3600);
    return String(buf);
}

// ═══════════════════════════════════════════════════════════════
// TrackerDataModule — data page only (reads g_tracker, no packets)
// ═══════════════════════════════════════════════════════════════

TrackerDataModule::TrackerDataModule()
    : MeshModule("TrackerData")
{
    LOG_INFO("TrackerDataModule: init\n");
}

// ── Data frame (scale-aware) ──────────────────────────────────
void TrackerDataModule::drawFrame(OLEDDisplay *display,
                                   OLEDDisplayUiState *state,
                                   int16_t x, int16_t y)
{
    const int16_t W = display->getWidth();
    const int16_t H = display->getHeight();

    // Title + divider
    display->setFont(ArialMT_Plain_16);
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->drawString(x + W / 2, y + 3, "TRACKER - Data");

    int16_t divY = y + (H / 9);
    display->drawLine(x, divY, x + W, divY);

    if (!g_tracker.valid) {
        display->setFont(ArialMT_Plain_16);
        display->setTextAlignment(TEXT_ALIGN_CENTER);
        display->drawString(x + W / 2, y + H / 2 - 8, "No data yet");
        return;
    }

    float lat        = g_tracker.latitude_i  / 1e7f;
    float lon        = g_tracker.longitude_i / 1e7f;
    float compassHdg = g_tracker.ground_track / 100.0f;
    float pdop       = g_tracker.PDOP / 100.0f;

    // Two-column layout
    const int16_t colL  = x + 5;
    const int16_t colR  = x + W / 2 + 5;
    const int16_t rowH  = (H - divY + y - 20) / 5;  // 5 data rows
    int16_t       ryL   = divY + 8;
    int16_t       ryR   = divY + 8;

    display->setFont(ArialMT_Plain_16);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    char buf[32];

    // ── Left column ───────────────────────────────────────────
    snprintf(buf, sizeof(buf), "Lat %.5f", lat);
    display->drawString(colL, ryL, buf); ryL += rowH;

    snprintf(buf, sizeof(buf), "Lon %.5f", lon);
    display->drawString(colL, ryL, buf); ryL += rowH;

    snprintf(buf, sizeof(buf), "Alt %dm", (int)g_tracker.altitude);
    display->drawString(colL, ryL, buf); ryL += rowH;

    if (g_tracker.motion_valid)
        snprintf(buf, sizeof(buf), "Spd %.1fkn", g_tracker.speed_kn);
    else
        snprintf(buf, sizeof(buf), "Spd ---");
    display->drawString(colL, ryL, buf); ryL += rowH;

    if (g_tracker.motion_valid)
        snprintf(buf, sizeof(buf), "COG %03.0f\xC2\xB0T", g_tracker.cog_deg);
    else
        snprintf(buf, sizeof(buf), "COG ---");
    display->drawString(colL, ryL, buf);

    // ── Right column ──────────────────────────────────────────
    snprintf(buf, sizeof(buf), "Hdg %03.0f\xC2\xB0M", compassHdg);
    display->drawString(colR, ryR, buf); ryR += rowH;

    snprintf(buf, sizeof(buf), "Sats %u", (unsigned)g_tracker.sats_in_view);
    display->drawString(colR, ryR, buf); ryR += rowH;

    snprintf(buf, sizeof(buf), "PDOP %.1f", pdop);
    display->drawString(colR, ryR, buf); ryR += rowH;

    snprintf(buf, sizeof(buf), "RSSI %ddBm", (int)g_tracker.rssi);
    display->drawString(colR, ryR, buf); ryR += rowH;

    // Window fill: SAR data maturity indicator (e.g. 8/20 = ~4min)
    snprintf(buf, sizeof(buf), "Win %u/20", g_tracker.window_fills);
    display->drawString(colR, ryR, buf);

    // Age — bottom right, small
    display->setFont(ArialMT_Plain_10);
    display->setTextAlignment(TEXT_ALIGN_RIGHT);
    display->drawString(x + W - 3, y + H - 14, formatAge(millis()));
}

// ── Age formatter ─────────────────────────────────────────────
String TrackerDataModule::formatAge(uint32_t now_ms)
{
    if (!g_tracker.valid || g_tracker.last_rx_ms == 0) return "--";
    uint32_t age_s = (now_ms - g_tracker.last_rx_ms) / 1000;
    char buf[12];
    if (age_s < 60)
        snprintf(buf, sizeof(buf), "%us ago", age_s);
    else if (age_s < 3600)
        snprintf(buf, sizeof(buf), "%um ago", age_s / 60);
    else
        snprintf(buf, sizeof(buf), "%uh ago", age_s / 3600);
    return String(buf);
}
