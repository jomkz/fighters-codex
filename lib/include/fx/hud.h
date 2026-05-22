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
    int16_t     value;
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

} // namespace fx
