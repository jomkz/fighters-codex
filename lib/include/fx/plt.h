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
// Stats block (0x1F80-0x21F7) — confirmed from RE:
//   Mission counters  0x1F80-0x1FAF  (12 x u32)
//   Kill tallies      0x1FB0-0x2017  (13 categories x 8 bytes: player u32 + wingman u32)
//   Unknown gap       0x2018-0x20B7  (0xA0 bytes — not accessed in decompile)
//   Weapon accuracy   0x20B8-0x21F7  (8 groups x 0x28 bytes: player slot + wingman slot)
//
// Medal-flag band (0x572-0x57C) — mapped statically from the per-campaign
// *Medals passes (P.md § Medal-flag band): eleven u8 decoration slots, one
// awarded-flag per medal except purple_hearts, which is a count 0-3.
// _AwardMedal appends each awarded title to the service record at 0x5AF
// (null-terminated strings, appended while the block stays under 1,000 bytes).
//
// Remaining gaps (0xB0-0xC1, 0xCF-0x571 + 0x57D-0x5AE, 0x2018-0x20B7,
// 0x21F8-0x25DF): no code in the game executable was found accessing these
// regions via static analysis. Differential save of fresh pilot files shows
// all zeros — populated only after actual campaign gameplay. Layout unknown;
// marked reserved (#29).

namespace fx {

// One slot of the campaign store table (0x1C60): 50 entries x 16 bytes.
//
// Not just weapons: the table also holds drop tanks (.GAS), sensor pods (.SEE) and
// ECM pods (.ECM) — everything the campaign lets a pilot draw from.
struct PltOrdnance {
    std::string name;      // "AIM9M.JT", "F250.GAS", "AAS38.SEE", "ALQ167.ECM"
    int16_t     quantity;  // 0x7FFF = unlimited (_AddCampaignStore returns early on it);
                           // -1 is carried by the gun entries in every campaign save
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
    uint32_t type3;   // role TBD — present in FA struct; confirmed 5th field is kills
    uint32_t kills;
};

struct PltWpnGroup {
    PltWpnSlot player;
    PltWpnSlot wingman;
};

// Confirmed stats block — requires file size >= 0x21F8.
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

// Medal-flag band (0x572-0x57C): one u8 per decoration slot, written by the
// per-campaign *Medals passes. Every field is a 0/1 awarded flag except
// purple_hearts (a count, capped at 3 in-engine). A slot means the same
// decoration class in every campaign, with the Navy or Air Force variant of
// the title by theater.
struct PltMedals {
    uint8_t honor;          // 0x572  Medal of Honor (Navy / Air Force)
    uint8_t cross;          // 0x573  Navy Cross / Air Force Cross
    uint8_t dsm;            // 0x574  Distinguished Service Medal
    uint8_t air_medal;      // 0x575  Air Medal
    uint8_t dfc;            // 0x576  Distinguished Flying Cross
    uint8_t achievement;    // 0x577  Achievement Medal
    uint8_t commendation;   // 0x578  Commendation Medal
    uint8_t expeditionary;  // 0x579  Expeditionary Medal
    uint8_t purple_hearts;  // 0x57A  Purple Heart count, 0-3
    uint8_t silver_star;    // 0x57B  Silver Star
    uint8_t yellow_fever;   // 0x57C  Conquest of Yellow Fever (Kurile easter egg)
};

// Parse pilot identity block. Returns false if size < 0xB0 or version tag != 0x0F.
bool plt_parse(const uint8_t* data, size_t size, PltInfo* info);

// Parse confirmed stats block. Returns false if size < 0x21F8 (stats not present).
bool plt_parse_stats(const uint8_t* data, size_t size, PltStats* stats);

// Parse the medal-flag band. Returns false if size < 0x57D (band not present).
bool plt_parse_medals(const uint8_t* data, size_t size, PltMedals* medals);

// Decoded view of the service-record block at 0x5AF: the null-terminated
// citation lines _AwardMedal appended, read sequentially (up to 10 entries,
// bounded by the campaign filename field at 0xD7F). A VIEW ONLY: the block's
// full content layout is still #29's mission-log gap, so plt_write never
// re-encodes it — these strings pass through verbatim.
std::vector<std::string> plt_service_record(const uint8_t* data, size_t size);

// A pilot file decoded for round-trip editing. `raw` holds every original byte
// verbatim — the pass-through backbone. `info` and `stats` are decoded views
// over the mapped regions (`stats` valid only when `has_stats`). Editing is
// done by mutating the views; `plt_write` overlays them back onto a copy of
// `raw`, so the four unmapped gap regions and the variable-length
// campaign/ordnance region are preserved exactly (see P.md § Round-Trip Notes).
struct PltFile {
    std::vector<uint8_t> raw;
    PltInfo   info;
    PltStats  stats{};
    bool      has_stats = false;
    PltMedals medals{};
    bool      has_medals = false;   // file reaches 0x57D
    // Decoded view of the citation lines at 0x5AF (see plt_service_record).
    // Never overlaid by plt_write — the surrounding mission-log region is
    // still unmapped (#29), so editing it is not supported yet.
    std::vector<std::string> service_record;
};

// Read a pilot file: keep the full bytes in `out->raw` and decode the identity
// (+ campaign) and stats views. Returns false if the identity block is invalid
// (same criteria as plt_parse); *out is unspecified on failure.
bool plt_read(const uint8_t* data, size_t size, PltFile* out);

// Serialize a pilot file: start from a copy of `f.raw` and overlay only the
// fixed-offset mapped fields — the identity block, (when `f.has_medals`) the
// medal-flag band, and (when `f.has_stats`) the stats counters. Every other
// byte passes through verbatim, so a
// plt_read → plt_write round-trip is byte-identical. Edit `f.info` / `f.stats`
// first to change those fields. Returns an empty vector if `f.raw` is shorter
// than the identity block (0xB0 bytes).
std::vector<uint8_t> plt_write(const PltFile& f);

// Read then write: a byte-identical round-trip for a valid pilot file, an empty
// vector for anything plt_read rejects. Convenience for the codec census.
std::vector<uint8_t> plt_repack(const uint8_t* data, size_t size);

} // namespace fx
