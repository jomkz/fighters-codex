#include "ft/t2.h"
#include <cstring>

namespace ft {

static const uint32_t HEADER_SIZE = 149;
static const uint32_t TILE_SIZE   = 195; // 65 * 3 bytes

// T2 header layout (all verified against 16 theater files):
//   0x00  "BIT2" magic
//   0x04  null-terminated theater display name
//   0x54  null-terminated associated .PIC filename
//   0x64  u32 LE  dim_x (grid columns)
//   0x7D  u8      dim_y (grid rows; byte inside what looks like a u32 field)
// Tile block starts immediately after the 149-byte header.
// Each tile: 195 bytes (65 records × 3 bytes). Record 0 byte 0 = surface class (0xFF=water).

bool t2_info(const uint8_t* data, size_t size, T2Info* info) {
    if (size < HEADER_SIZE) return false;
    if (memcmp(data, "BIT2", 4) != 0) return false;

    uint32_t dim_x = data[0x64] | (data[0x65] << 8) | (data[0x66] << 16) | (data[0x67] << 24);
    uint32_t dim_y = data[0x7D];

    if (dim_x == 0 || dim_y == 0) return false;

    uint64_t expected = HEADER_SIZE + (uint64_t)dim_x * dim_y * TILE_SIZE;
    if ((uint64_t)size != expected) return false;

    info->dim_x       = dim_x;
    info->dim_y       = dim_y;
    info->tile_count  = dim_x * dim_y;
    info->tile_offset = HEADER_SIZE;

    info->surface_dist.clear();
    for (uint32_t t = 0; t < info->tile_count; t++) {
        size_t tile_base = HEADER_SIZE + (size_t)t * TILE_SIZE;
        uint8_t surface_class = data[tile_base];
        info->surface_dist[surface_class]++;
    }

    return true;
}

} // namespace ft
