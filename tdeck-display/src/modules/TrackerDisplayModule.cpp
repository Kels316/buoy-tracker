#include "TrackerDisplayModule.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "channels.h"
#include "configuration.h"
#include "graphics/Screen.h"
#include "mesh/generated/meshtastic/mesh.pb.h"
#include "mesh/generated/meshtastic/telemetry.pb.h"
#include <Arduino.h>
#include <math.h>
#include <pb_decode.h>

TrackerDisplayModule *trackerDisplayModule;

// ── Constructor ───────────────────────────────────────────────
TrackerDisplayModule::TrackerDisplayModule()
    : SinglePortModule("TrackerDisplay", meshtastic_PortNum_POSITION_APP)
{
    LOG_INFO("TrackerDisplayModule: initialised, listening on POSITION_APP\n");
}

// ── Packet handler ────────────────────────────────────────────
ProcessMessage TrackerDisplayModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    // Only process packets from channel 0 (our private TRACKER channel)
    if (mp.channel != 0) return ProcessMessage::CONTINUE;

    // Decode the position protobuf
    meshtastic_Position pos = meshtastic_Position_init_default;
    pb_istream_t stream = pb_istream_from_buffer(
        mp.decoded.payload.bytes, mp.decoded.payload.size);

    if (!pb_decode(&stream, &meshtastic_Position_msg, &pos)) {
        LOG_WARN("TrackerDisplayModule: failed to decode position\n");
        return ProcessMessage::CONTINUE;
    }

    // Store the data
    tracker.latitude_i   = pos.latitude_i;
    tracker.longitude_i  = pos.longitude_i;
    tracker.altitude     = pos.altitude;
    tracker.ground_track = pos.ground_track;
    tracker.sats_in_view = pos.sats_in_view;
    tracker.PDOP         = pos.PDOP;
    tracker.time         = pos.time;
    tracker.last_rx_ms   = millis();
    tracker.rssi         = mp.rx_rssi;
    tracker.node_id      = mp.from;
    tracker.valid        = true;

    LOG_INFO("TrackerDisplayModule: position updated from node 0x%08x "
             "lat=%d lon=%d track=%u\n",
             mp.from, pos.latitude_i, pos.longitude_i, pos.ground_track);

    // Request screen refresh
    if (screen)
        screen->forceDisplay();

    return ProcessMessage::CONTINUE;
}

// ── Frame dispatcher ──────────────────────────────────────────
// Meshtastic calls drawFrame for each of our registered pages.
// currentPage is set by the frame index passed via state->currentFrame
// mapped against getNumExtraFrames().
void TrackerDisplayModule::drawFrame(OLEDDisplay *display,
                                     OLEDDisplayUiState *state,
                                     int16_t x, int16_t y)
{
    // Determine which of our two pages we are drawing.
    // Meshtastic passes the global frame index; we track which page
    // we're on by checking if we've been called an even or odd number
    // of times within the frame cycle — simplest approach is to store
    // currentPage and toggle it based on the frame index parity.
    currentPage = (state->currentFrame % 2);

    if (currentPage == 0)
        drawMapPage(display, state, x, y);
    else
        drawDataPage(display, state, x, y);
}

// ── Map page ──────────────────────────────────────────────────
void TrackerDisplayModule::drawMapPage(OLEDDisplay *display,
                                       OLEDDisplayUiState *state,
                                       int16_t x, int16_t y)
{
    display->clear();
    display->setFont(ArialMT_Plain_10);

    // Header bar
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->drawString(x + 64, y + 0, "TRACKER — Map");
    display->drawLine(x + 0, y + 12, x + 128, y + 12);

    if (!tracker.valid) {
        display->setTextAlignment(TEXT_ALIGN_CENTER);
        display->drawString(x + 64, y + 30, "Waiting for");
        display->drawString(x + 64, y + 42, "tracker packet...");
        return;
    }

    // Map area: 128x40 pixels below the header
    const int16_t mapX = x + 4;
    const int16_t mapY = y + 15;
    const int16_t mapW = 120;
    const int16_t mapH = 38;

    display->drawRect(mapX, mapY, mapW, mapH);

    // Draw crosshair centre dot (tracker position — fixed centre for now)
    const int16_t cx = mapX + mapW / 2;
    const int16_t cy = mapY + mapH / 2;
    display->fillCircle(cx, cy, 3);

    // Draw heading arrow
    float headingDeg = (float)(tracker.ground_track) / 100.0f;
    drawArrow(display, cx, cy, 12, headingDeg);

    // Coordinates below map
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    char latBuf[24], lonBuf[24];
    float lat = tracker.latitude_i / 1e7f;
    float lon = tracker.longitude_i / 1e7f;
    snprintf(latBuf, sizeof(latBuf), "Lat: %.5f", lat);
    snprintf(lonBuf, sizeof(lonBuf), "Lon: %.5f", lon);
    display->drawString(x + 2, y + 54, latBuf);
    display->drawString(x + 2, y + 64, lonBuf);

    // Age top right
    display->setTextAlignment(TEXT_ALIGN_RIGHT);
    display->drawString(x + 126, y + 54, formatAge(millis()));
}

// ── Data page ─────────────────────────────────────────────────
void TrackerDisplayModule::drawDataPage(OLEDDisplay *display,
                                        OLEDDisplayUiState *state,
                                        int16_t x, int16_t y)
{
    display->clear();
    display->setFont(ArialMT_Plain_10);
    display->setTextAlignment(TEXT_ALIGN_LEFT);

    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->drawString(x + 64, y + 0, "TRACKER — Data");
    display->drawLine(x + 0, y + 12, x + 128, y + 12);

    if (!tracker.valid) {
        display->setTextAlignment(TEXT_ALIGN_CENTER);
        display->drawString(x + 64, y + 30, "No data yet");
        return;
    }

    display->setTextAlignment(TEXT_ALIGN_LEFT);

    float lat     = tracker.latitude_i / 1e7f;
    float lon     = tracker.longitude_i / 1e7f;
    float heading = tracker.ground_track / 100.0f;
    float pdop    = tracker.PDOP / 100.0f;

    char buf[48];
    int row = y + 15;
    const int rowH = 11;

    snprintf(buf, sizeof(buf), "Lat:  %.5f", lat);
    display->drawString(x + 2, row, buf); row += rowH;

    snprintf(buf, sizeof(buf), "Lon:  %.5f", lon);
    display->drawString(x + 2, row, buf); row += rowH;

    snprintf(buf, sizeof(buf), "Alt:  %dm", (int)tracker.altitude);
    display->drawString(x + 2, row, buf);

    // Right column
    row = y + 15;
    snprintf(buf, sizeof(buf), "Hdg:%.1f", heading);
    display->setTextAlignment(TEXT_ALIGN_RIGHT);
    display->drawString(x + 126, row, buf); row += rowH;

    snprintf(buf, sizeof(buf), "Sats:%u", (unsigned)tracker.sats_in_view);
    display->drawString(x + 126, row, buf); row += rowH;

    snprintf(buf, sizeof(buf), "PDOP:%.1f", pdop);
    display->drawString(x + 126, row, buf); row += rowH;

    // Bottom row: RSSI and age
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    snprintf(buf, sizeof(buf), "RSSI:%ddBm", (int)tracker.rssi);
    display->drawString(x + 2, row, buf);

    display->setTextAlignment(TEXT_ALIGN_RIGHT);
    display->drawString(x + 126, row, formatAge(millis()));
}

// ── Heading arrow ─────────────────────────────────────────────
// Draws a simple arrow centred at (cx,cy) pointing in angleDeg
// (0 = north = up, clockwise positive)
void TrackerDisplayModule::drawArrow(OLEDDisplay *display,
                                     int16_t cx, int16_t cy,
                                     int16_t r, float angleDeg)
{
    // Convert to screen angle: 0 = up, clockwise
    float rad = (angleDeg - 90.0f) * (M_PI / 180.0f);

    // Arrow tip
    int16_t tx = cx + (int16_t)(r * cosf(rad));
    int16_t ty = cy + (int16_t)(r * sinf(rad));

    // Arrow tail
    int16_t bx = cx - (int16_t)((r * 0.6f) * cosf(rad));
    int16_t by = cy - (int16_t)((r * 0.6f) * sinf(rad));

    // Arrow head wings
    float wingRad = rad + M_PI * 0.8f;
    int16_t w1x = tx + (int16_t)(5 * cosf(wingRad));
    int16_t w1y = ty + (int16_t)(5 * sinf(wingRad));
    float wingRad2 = rad - M_PI * 0.8f;
    int16_t w2x = tx + (int16_t)(5 * cosf(wingRad2));
    int16_t w2y = ty + (int16_t)(5 * sinf(wingRad2));

    display->drawLine(bx, by, tx, ty);
    display->drawLine(tx, ty, w1x, w1y);
    display->drawLine(tx, ty, w2x, w2y);
}

// ── Age formatter ─────────────────────────────────────────────
String TrackerDisplayModule::formatAge(uint32_t now_ms)
{
    if (!tracker.valid || tracker.last_rx_ms == 0)
        return "--";

    uint32_t age_s = (now_ms - tracker.last_rx_ms) / 1000;
    char buf[12];
    if (age_s < 60)
        snprintf(buf, sizeof(buf), "%us ago", age_s);
    else if (age_s < 3600)
        snprintf(buf, sizeof(buf), "%um ago", age_s / 60);
    else
        snprintf(buf, sizeof(buf), "%uh ago", age_s / 3600);
    return String(buf);
}
