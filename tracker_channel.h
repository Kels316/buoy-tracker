#pragma once

/**
 * tracker_channel.h
 * 
 * Bakes a private channel + PSK into the firmware at compile time.
 * The setupTrackerChannel() function should be called once from setup()
 * BEFORE the mesh service starts.
 *
 * HOW TO CHANGE YOUR KEY:
 *   1. Generate a random 32-byte key (e.g. `openssl rand -base64 32`)
 *   2. Convert each byte to 0x?? hex and replace TRACKER_PSK below.
 *   3. Rebuild and reflash all nodes that need to communicate.
 */

#include "NodeDB.h"
#include "mesh/generated/meshtastic/channel.pb.h"
#include <string.h>

// -------------------------------------------------------------------
// YOUR PRIVATE CHANNEL SETTINGS — edit these before building
// -------------------------------------------------------------------

// Channel name (max 11 chars)
#define TRACKER_CHANNEL_NAME  "TRACKER"

// 256-bit (32-byte) pre-shared key
// Replace with your own key — every node must share this exact key.
static const uint8_t TRACKER_PSK[32] = {
    0x4a, 0x3f, 0x8c, 0x21, 0xd7, 0x55, 0xb2, 0x09,
    0xe1, 0x7a, 0x44, 0xfc, 0x30, 0x8e, 0x6b, 0xd3,
    0x92, 0x1c, 0x5f, 0xa8, 0x77, 0x03, 0xe6, 0x4d,
    0xbb, 0x29, 0x10, 0x58, 0xc4, 0x9d, 0x6e, 0xf1
};

// -------------------------------------------------------------------

inline void setupTrackerChannel()
{
    meshtastic_ChannelSettings cs = meshtastic_ChannelSettings_init_default;

    // Name
    strncpy(cs.name, TRACKER_CHANNEL_NAME, sizeof(cs.name) - 1);

    // PSK
    cs.psk.size = sizeof(TRACKER_PSK);
    memcpy(cs.psk.bytes, TRACKER_PSK, sizeof(TRACKER_PSK));

    // Modem preset — use LongFast for range; change to LongSlow for max range
    cs.modem_preset = meshtastic_ChannelSettings_ModemPreset_LONG_FAST;

    // Write to channel slot 0 and mark as PRIMARY
    meshtastic_Channel ch  = meshtastic_Channel_init_default;
    ch.settings            = cs;
    ch.role                = meshtastic_Channel_Role_PRIMARY;
    ch.index               = 0;

    channels.setChannel(ch);
    channels.onConfigChanged(); // persist to flash
}
