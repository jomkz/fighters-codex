#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Pilot save file parser (.P files, e.g. PLT441.P).
//
// Identity block layout (fully mapped, offsets 0x00-0xAF):
//   0x00  1   u8      type/version tag (observed: 0x0F)
//   0x01  63  char[]  pilot name, null-padded
//   0x40  32  char[]  callsign, null-padded
//   0x61  13  char[]  callsign voice file (e.g. "^ACID.5K"), null-padded
//   0x6E  13  char[]  nose art ID (e.g. "NOSE01"), null-padded
//   0x7B  13  char[]  left wing decal ID (e.g. "LEFT03"), null-padded
//   0x88  13  char[]  right wing decal ID (e.g. "RIGHT03"), null-padded
//   0x95  13  char[]  pilot portrait ID (e.g. "PILOT02"), null-padded
//   0xA2  14  char[]  rank string (e.g. "2nd Lieutenant"), null-padded
//
// Ordnance inventory (0x1C60): 50 entries x 16 bytes each.
//
// Stats block (0x1F80-0x21F7) â€” confirmed from RE:
//   Mission counters  0x1F80-0x1FAF  (12 x u32)
//   Kill tallies      0x1FB0-0x2017  (13 categories x 8 bytes: player u32 + wingman u32)
//   Unknown gap       0x2018-0x20B7  (0xA0 bytes â€” not accessed in decompile)
//   Weapon accuracy   0x20B8-0x21F7  (8 groups x 0x28 bytes: player slot + wingman slot)
//
// Remaining gaps (0xB0-0xC1, 0xCF-0x5AE, 0x21F8-0x25DF):
//   No code in FA.EXE was found accessing these regions via static analysis.
//   Differential save of fresh pilot files shows all zeros â€” populated only after
//   actual campaign gameplay. Layout unknown; marked reserved.

namespace fx {

struct PltOrdnance {
    std::string jt_name;  // e.g. "AIM9M.JT"
    uint8_t     quantity;
};

struct PltInfo {
    uint8_t     version_tag;
    std::string name;
    std::string callsign;
    std::string voice_file;
    std::string nose_art;
    std::string left_decal;
    std::string right_decal;
    std::string portrait;
    std::string rank;

    // Campaign block (empty if pilot has no active campaign)
    std::string cam_file;      // e.g. "EGYPT.CAM"
    std::string cam_name;      // e.g. "Egypt 1998"
    std::string aircraft;      // e.g. "F22.PT"
    std::vector<std::string>  aircraft_pool; // available aircraft .PT refs
    std::vector<PltOrdnance>  ordnance;      // loaded weapons
    std::vector<std::string>  sensors;       // .SEE and .ECM refs
};

// Kill category (0x1FB0-0x2017): player and wingman u32 kill counts.
struct PltKill {
    uint32_t player;
    uint32_t wingman;
};

// Weapon accuracy slot: 5 u32 fields per player/wingman.
struct PltWpnSlot {
    uint32_t damage_total;
    uint32_t shots_fired;
    uint32_t hits;
    uint32_t type3;   // role TBD â€” present in FA struct; confirmed 5th field is kills
    uint32_t kills;
};

struct PltWpnGroup {
    PltWpnSlot player;
    PltWpnSlot wingman;
};

// Confirmed stats block â€” requires file size >= 0x21F8.
struct PltStats {
    // Mission and loss counters (0x1F80-0x1FAF)
    uint32_t missions_flown;
    uint32_t wingman_missions;
    uint32_t missions_failed;
    uint32_t shots_fired_total;
    uint32_t ejections;
    uint32_t wingman_kia;
    uint32_t player_damage_pct;
    uint32_t wingman_damage_pct;
    uint32_t player_landings;
    uint32_t wingman_landings;
    uint32_t player_landing_score;
    uint32_t wingman_landing_score;

    // Kill tallies (0x1FB0-0x2017), 13 categories
    PltKill kills_air_fighter;      // obj_class 0x8000
    PltKill kills_air_fighter_b;    // obj_class 0x4000
    PltKill kills_air_crash;        // aircraft by crash or BA weapon
    PltKill kills_naval;            // obj_class 0x2000
    PltKill kills_sam;              // obj_class 0x1000
    PltKill kills_aaa;              // obj_class 0x800
    PltKill kills_armor;            // obj_class 0x400
    PltKill kills_apc;              // obj_class 0x200
    PltKill kills_vehicle;          // obj_class 0x100
    PltKill kills_infantry;         // obj_class 0x40
    PltKill kills_friendly_fire;
    PltKill kills_air_nonfighter;   // aerial, non-0x8000
    PltKill kills_capital_ship;     // naval + hitpoints > 999

    // Weapon accuracy groups (0x20B8-0x21F7), 8 groups
    PltWpnGroup wpn_aa_gun;         // OBJ_TYPE bit 0x10000
    PltWpnGroup wpn_aa_missile;     // OBJ_TYPE bit 0x20000
    PltWpnGroup wpn_ground;         // OBJ_TYPE bits 0x20080
    PltWpnGroup wpn_naval;          // OBJ_TYPE bit 0x10
    PltWpnGroup wpn_kill_aircraft;  // shooter = obj byte 0 0x04
    PltWpnGroup wpn_kill_b;
    PltWpnGroup wpn_kill_c;
    PltWpnGroup wpn_kill_d;
};

// Parse pilot identity block. Returns false if size < 0xB0 or version tag != 0x0F.
bool plt_parse(const uint8_t* data, size_t size, PltInfo* info);

// Parse confirmed stats block. Returns false if size < 0x21F8 (stats not present).
bool plt_parse_stats(const uint8_t* data, size_t size, PltStats* stats);

} // namespace fx
