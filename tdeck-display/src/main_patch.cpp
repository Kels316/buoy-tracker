// =============================================================
//  main_patch.cpp  —  DO NOT compile directly
//
//  Shows the TWO edits needed in src/main.cpp of the
//  Meshtastic firmware repo to wire in the tracker display.
// =============================================================


// ── EDIT 1 ── Add near the top of main.cpp with other includes
//
#include "modules/TrackerDisplayModule.h"
//
// ─────────────────────────────────────────────────────────────


// ── EDIT 2 ── Inside void setup(), after nodeDB.init()
//             and before service.init(), add:
//
    // --- Tracker display ---
    trackerDisplayModule = new TrackerDisplayModule();
    // --- end tracker display ---
//
// ─────────────────────────────────────────────────────────────


// ── NOTHING ELSE needs to change ─────────────────────────────
// TrackerDisplayModule registers itself with the screen system
// automatically via the SinglePortModule base class.
// The two extra pages will appear in the normal T-Deck screen
// cycle alongside all built-in Meshtastic screens.
// Navigate between pages using the existing T-Deck screen
// controls (trackball click or keyboard).
