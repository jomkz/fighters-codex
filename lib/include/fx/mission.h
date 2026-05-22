#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// .M (Mission) and .MM (Mission Map) file format.
//
// Plain ASCII text.  First line is "textFormat" (no brackets).
// Each top-level keyword is one-per-line; indented lines are sub-fields.
// Object blocks: "obj" keyword opens a block, "." on its own line closes it.
//
// Common keywords:
//   textFormat          -- magic header
//   brief / briefmap / armplane / selectplane  -- screen flags
//   map <file>          -- terrain / map reference
//   layer <file> <idx>  -- LAY file reference
//   clouds <n>          -- cloud coverage
//   wind <dir> <speed>
//   view <x> <y> <z>
//   time <h> <m>
//   sides4              -- nationality table (indented $XX values follow)
//   usGroundSkill / usAirSkill / themGroundSkill / themAirSkill <n>
//   historicalera <n>
//   obj                 -- start of object block
//     type <OT/NT/PT file>
//     pos <x> <y> <z>
//     angle <p> <b> <r>
//     nationality3 <n>
//     flags <hex>
//     speed <n>
//     alias <n>
//     name <string>
//     skill <n>
//     react <a> <b> <c>
//     .                 -- end of object block

namespace fx {

struct MissionObj {
    std::string type_file;   // e.g. "KIEV.NT"
    int64_t     pos[3];      // x, y, z world coordinates
    int         angle[3];    // pitch, bank, roll degrees
    int         nationality;
    int64_t     flags;
    int         speed;
    int         alias;
    std::string name;        // optional name label
};

struct MissionInfo {
    bool        is_mission;  // true = .M, false = .MM
    std::string map_file;    // terrain reference
    std::string layer_file;
    int         layer_index;
    int         clouds;
    int         wind_dir, wind_speed;
    int         time_h, time_m;
    int         obj_count;
    std::vector<std::string> screen_flags; // "brief", "briefmap", "armplane", etc.
};

// Parse summary info from a mission/map file.
MissionInfo mission_parse_info(const uint8_t* data, size_t size);

// Round-trip: parse raw lines and re-emit (verbatim copy with CRLF normalization).
std::vector<uint8_t> mission_roundtrip(const uint8_t* data, size_t size);

} // namespace fx
