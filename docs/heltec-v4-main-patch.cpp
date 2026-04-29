// =============================================================
//  heltec-v4-main-patch.cpp  —  DO NOT compile directly
//
//  This file shows the TWO edits you need to make inside the
//  upstream Meshtastic source to wire in the tracker module.
//  Both edits are in:  src/main.cpp  (inside the firmware repo)
// =============================================================


// ── EDIT 1 ── Add these includes near the top of main.cpp ────
//
#include "modules/TrackerModule.h"
#include "tracker_channel.h"
//
// ─────────────────────────────────────────────────────────────


// ── EDIT 2 ── Inside void setup(), AFTER nodeDB.init() ───────
//             and BEFORE service.init(), add:
//
    // --- Heltec V4 Tracker customisation ---
    setupTrackerChannel();          // bake in private PSK + GPS pins
    trackerModule = new TrackerModule();  // start 30s GPS sender
    strobeModule  = new StrobeModule();   // start recovery strobe
    // --- end tracker customisation ---
//
// ─────────────────────────────────────────────────────────────


// ── NOTHING ELSE needs to change in main.cpp ─────────────────
// GPS is wired to the dedicated GNSS connector (SH1.25-8P):
//   GPIO 38 = RX into the board (from GPS TX)
//   GPIO 39 = TX from the board (to GPS RX)
// The OSThread inside TrackerModule self-schedules every 30 s.
// The power LED is driven from the TrackerModule constructor.
// The TX LED flashes inside sendPosition().
// The strobe fires on GPIO 5 via MOSFET (NOT GPIO 13 — LORA_BUSY).
