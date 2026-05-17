#pragma once
#include <cstddef>
#include <cstdint>
#include <map>

// T2 terrain map parser.
//
// File layout (149-byte header, verified against 16 theater files):
//   0x00   4    Magic "BIT2"
//   0x04   *    Null-terminated theater display name
//   0x54   *    Null-terminated associated .PIC filename
//   0x64   4    dim_x (u32 LE, grid columns)
//   0x7D   1    dim_y (u8, grid rows)
//   0x95  N*195 Tile blocks: each tile = 65 * 3-byte records
//              Record 0 byte 0 = surface_class (0xFF=water)

namespace ft {

struct T2Info {
    uint32_t dim_x;       // grid columns
    uint32_t dim_y;       // grid rows
    uint32_t tile_count;  // dim_x * dim_y
    uint32_t tile_offset; // always 149

    // Surface class distribution: key = surface_class byte, value = tile count
    std::map<uint8_t, uint32_t> surface_dist;
};

// Parse T2 header and build surface class distribution.
// Returns false if magic doesn't match or file size is inconsistent.
bool t2_info(const uint8_t* data, size_t size, T2Info* info);

} // namespace ft
