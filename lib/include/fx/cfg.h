#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

// CFG — game configuration (see CFG.md). EA.CFG is the 347-byte binary
// CONFIG struct written by UCONFIG_save_EA_CFG (0x4b2980): magic 0x24 and
// exact size 0x15B or the engine falls back to defaults. Every field below
// is mapped from the load/save decompiles; the three fields the RE hasn't
// traced (+0x004, +0x008, +0x0E2 — gap #54) pass through verbatim, and the
// string fields keep their full raw bytes (the engine only null-checks the
// first byte), so cfg_write is the byte-identical inverse of cfg_read.

namespace fx {

constexpr size_t   EA_CFG_SIZE  = 347;   // 0x15B
constexpr uint32_t EA_CFG_MAGIC = 0x24;

struct EaCfg {
    uint32_t magic;              // +0x000 — always 0x24
    uint32_t unk_004;            // +0x004 — untraced (#54), pass-through
    uint32_t unk_008;            // +0x008 — untraced (#54), pass-through
    uint32_t stick_device;       // +0x00C
    uint32_t rudder_device;      // +0x010
    uint32_t throttle_device;    // +0x014
    uint32_t throttle_100;       // +0x018 — 100% throttle calibration
    uint32_t axis_map[48];       // +0x01C — joystick axis/button table
    uint8_t  window_types[6];    // +0x0DC — display window mode per view
    uint8_t  unk_0e2;            // +0x0E2 — untraced (#54), pass-through
    uint8_t  sound_on;           // +0x0E3
    uint8_t  stereo_swap;        // +0x0E4
    uint16_t overall_vol;        // +0x0E5
    uint16_t engine_vol;         // +0x0E7
    uint16_t lock_vol;           // +0x0E9
    uint16_t rwr_vol;            // +0x0EB
    uint16_t stall_vol;          // +0x0ED
    uint16_t radio_vol;          // +0x0EF
    uint16_t flight_music_vol;   // +0x0F1
    uint16_t other_music_vol;    // +0x0F3
    uint16_t stereo_separation;  // +0x0F5
    uint8_t  midi_device;        // +0x0F7
    uint32_t game_prefs;         // +0x0F8
    uint32_t game_multi_prefs;   // +0x0FC
    uint32_t game_debug_prefs;   // +0x100
    uint32_t hud_brightness;     // +0x104
    uint8_t  campaign_pilot[33]; // +0x108 — raw bytes (null-terminated in use)
    uint8_t  callsign[32];       // +0x129 — multiplayer callsign
    uint8_t  squadron[13];       // +0x149 — squadron / wing tag
    uint32_t glasses3d_amount;   // +0x156
    uint8_t  ad_count;           // +0x15A
};

// Parse EA.CFG. false unless size == 347 and magic == 0x24 (the engine's
// own validation).
bool cfg_read(const uint8_t* data, size_t size, EaCfg& out);

// Serialize — exactly 347 bytes; byte-identical inverse of cfg_read.
std::vector<uint8_t> cfg_write(const EaCfg& cfg);

} // namespace fx
