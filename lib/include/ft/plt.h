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
// Campaign block: starts near 0x0D7F for active pilots.
// Contains null-terminated strings: CAM filename, campaign display name,
// aircraft .PT ref, available aircraft pool, ordnance (.JT + u8 quantity), sensors.
//
// Stats block (0xB0 - ~0x0D7E): not yet mapped.

namespace ft {

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

// Parse a pilot save file. Returns false if size < 0xB0 or version tag != 0x0F.
bool plt_parse(const uint8_t* data, size_t size, PltInfo* info);

} // namespace ft
