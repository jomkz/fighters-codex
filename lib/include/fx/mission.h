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

// One "key value..." line, values kept as raw text tokens. Objects and
// waypoints carry a per-type-varying field list (M.md § Object block), so the
// generic form preserves every field without a fixed schema; the universal
// geometry fields are also promoted to typed members for convenience.
struct MissionField {
    std::string              key;
    std::vector<std::string> values;
};

// One waypoint inside an object's trailing `waypoint2` block. Opened by
// `w_index`; carries the rest of its `w_*` fields (w_pos2, w_goal, w_next,
// w_react, w_preferredTargetId2, w_speed, ...) verbatim.
struct MissionWaypoint {
    int                        index = 0;   // w_index
    std::vector<MissionField>  fields;      // every other w_* field, in order

    // First value list for `key` (e.g. "w_pos2"), or nullptr if absent.
    const std::vector<std::string>* get(const std::string& key) const;
};

// A placed object (`obj … .` block). type/pos/angle are typed; every other
// field is preserved in `fields`.
struct MissionObj {
    std::string                 type_file;  // `type` (e.g. "KIEV.NT")
    int64_t                     pos[3] = {0, 0, 0};    // `pos` x y z
    int                         angle[3] = {0, 0, 0};  // `angle` pitch bank roll
    std::vector<MissionField>   fields;     // nationality2, flags, alias, skill, react, ...

    // First value list for `key` (e.g. "alias"), or nullptr if absent.
    const std::vector<std::string>* get(const std::string& key) const;
};

// A `waypoint2 N` block: an ordered run of waypoints. In the stock files these
// blocks are grouped after *all* objects (never interleaved), and there are
// always fewer blocks than objects — so which object a block belongs to is NOT
// encoded adjacently. That ownership rule is unresolved (see M.md § Open
// Questions), so blocks are returned as a parallel list, not nested in objects.
struct MissionWaypointBlock {
    int                          count = 0;  // the `waypoint2 N` header value
    std::vector<MissionWaypoint> waypoints;
};

// Full placed-object decode: the object list and the waypoint blocks, both in
// file order.
struct MissionObjects {
    std::vector<MissionObj>           objects;
    std::vector<MissionWaypointBlock> waypoint_blocks;
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

// Parse the placed-object list and the waypoint blocks. type/pos/angle are
// promoted to typed members; all other fields (per-type-varying) are kept in
// `fields` verbatim, so nothing is lost. Both lists are in file order.
MissionObjects mission_parse_objects(const uint8_t* data, size_t size);

} // namespace fx
