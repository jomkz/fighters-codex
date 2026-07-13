#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Parser for FA .HUD cockpit heads-up display files.
// Format: Win32 PE DLL (Phar Lap LE variant). CODE section is a pure data
// struct -- no x86 code. Fixed size 0x2BB (699 bytes) across all aircraft.

namespace fx {

struct HudParam {
    std::string gauge;
    std::string field;
    // The tape gauges store their parameters as 32-bit, sign-extended (verified across
    // all 46 shipped HUDs); the rest are 16- or 8-bit. A 16-bit value cannot hold them,
    // and writing one back 16 bits wide left the high half stale — see hud.cpp.
    int32_t     value;
};

struct HudFile {
    bool valid = false;
    std::vector<std::string> asset_strings;  // ~<ac>, ~<ac>h, hudsym, winfont, etc.
    std::string icon_a;   // first advisory icon label  -- "GEAR"  (bit 0x100, gear actuator)
    std::string icon_b;   // second advisory icon label -- "FLAP"  (bit 0x080, flap actuator)
    std::string icon_c;   // third advisory icon label  -- "BRAKE" (bit 0x040, speedbrake)
    std::string icon_d;   // fourth advisory icon label -- "HOOK" or "BAY" (bit 0x200/0x400)
    std::vector<HudParam> params;
};

HudFile hud_parse(const uint8_t* data, size_t size);

// Rebuild a HUD DLL around edited gauge parameters and advisory icon
// labels (#99). `hud.params` must hold exactly one entry per known gauge
// field (any order); icon labels fit their fixed 8-byte slots (a NUL is
// kept when shorter than 8). Every byte the parser does not model — PE
// headers, asset strings, reserved regions — carries over verbatim, so an
// unedited parse→repack is byte-identical. Returns empty on
// unknown/missing/duplicate params, out-of-range values, or oversized
// icon labels.
std::vector<uint8_t> hud_repack(const uint8_t* orig, size_t orig_size,
                                const HudFile& hud);

} // namespace fx
