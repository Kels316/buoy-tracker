// =============================================================
//  main_patch.cpp  —  DO NOT compile directly
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
    // --- Tracker customisation ---
    setupTrackerChannel();          // bake in private PSK
    trackerModule = new TrackerModule();  // start 30s GPS sender
    // --- end tracker customisation ---
//
// ─────────────────────────────────────────────────────────────


// ── NOTHING ELSE needs to change in main.cpp ─────────────────
// The OSThread inside TrackerModule self-schedules every 30 s.
// The power LED is driven from the TrackerModule constructor.
// The TX LED flashes inside sendPosition().
