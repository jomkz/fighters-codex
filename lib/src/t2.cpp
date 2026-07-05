#include "fx/t2.h"
#include <cstring>

namespace fx {

static uint32_t rd32(const uint8_t* p) {
    return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24));
}

// Header field map in fx/t2.h; layout derived from the engine's T_Load
// (relocates the two array offsets into pointers) and T_GetLeaf (row-major
// (y*W + x)*3 indexing into each array).
bool t2_info(const uint8_t* data, size_t size, T2Info* info) {
    if (size < 0x95) return false;
    if (memcmp(data, "BIT2", 4) != 0) return false;

    uint32_t leaf_step   = rd32(data + 0x79);
    uint32_t tiles_w     = rd32(data + 0x7D);
    uint32_t tiles_h     = rd32(data + 0x81);
    uint32_t summary_off = rd32(data + 0x85);
    uint32_t leaves_w    = rd32(data + 0x89);
    uint32_t leaves_h    = rd32(data + 0x8D);
    uint32_t leaf_off    = rd32(data + 0x91);

    if (leaf_step == 0 || tiles_w == 0 || tiles_h == 0) return false;
    if (leaves_w != tiles_w * leaf_step || leaves_h != tiles_h * leaf_step)
        return false;

    // The two flat arrays account for the whole payload.
    uint64_t leaf_bytes    = (uint64_t)leaves_w * leaves_h * 3;
    uint64_t summary_bytes = (uint64_t)tiles_w * tiles_h * 3;
    if (leaf_off + leaf_bytes != summary_off) return false;
    if (summary_off + summary_bytes != (uint64_t)size) return false;

    info->dim_x          = tiles_w;
    info->dim_y          = tiles_h;
    info->tile_count     = tiles_w * tiles_h;
    info->leaf_step      = leaf_step;
    info->leaves_w       = leaves_w;
    info->leaves_h       = leaves_h;
    info->leaf_offset    = leaf_off;
    info->summary_offset = summary_off;

    info->surface_dist.clear();
    for (uint32_t t = 0; t < info->tile_count; t++) {
        uint8_t surface_class = data[(size_t)summary_off + (size_t)t * 3];
        info->surface_dist[surface_class]++;
    }

    return true;
}

} // namespace fx
