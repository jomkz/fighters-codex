#include "fx/t2.h"
#include <cstring>

namespace fx {

static uint32_t rd32(const uint8_t* p) {
    return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24));
}

// Header fields shared by t2_info and t2_read.
struct T2Header {
    uint32_t leaf_step;
    uint32_t tiles_w, tiles_h;
    uint32_t leaves_w, leaves_h;
    uint32_t leaf_off, summary_off;
};

// Header field map in fx/t2.h; layout derived from the engine's T_Load
// (relocates the two array offsets into pointers) and T_GetLeaf (row-major
// (y*W + x)*3 indexing into each array).
static bool parse_header(const uint8_t* data, size_t size, T2Header* h) {
    if (size < 0x95) return false;
    if (memcmp(data, "BIT2", 4) != 0) return false;

    h->leaf_step   = rd32(data + 0x79);
    h->tiles_w     = rd32(data + 0x7D);
    h->tiles_h     = rd32(data + 0x81);
    h->summary_off = rd32(data + 0x85);
    h->leaves_w    = rd32(data + 0x89);
    h->leaves_h    = rd32(data + 0x8D);
    h->leaf_off    = rd32(data + 0x91);

    if (h->leaf_step == 0 || h->tiles_w == 0 || h->tiles_h == 0) return false;
    if (h->leaves_w != h->tiles_w * h->leaf_step ||
        h->leaves_h != h->tiles_h * h->leaf_step)
        return false;

    // The two flat arrays follow the header and account for the whole payload.
    if (h->leaf_off < 0x95) return false;
    uint64_t leaf_bytes    = (uint64_t)h->leaves_w * h->leaves_h * 3;
    uint64_t summary_bytes = (uint64_t)h->tiles_w * h->tiles_h * 3;
    if (h->leaf_off + leaf_bytes != h->summary_off) return false;
    if (h->summary_off + summary_bytes != (uint64_t)size) return false;

    return true;
}

bool t2_info(const uint8_t* data, size_t size, T2Info* info) {
    T2Header h;
    if (!parse_header(data, size, &h)) return false;

    info->dim_x          = h.tiles_w;
    info->dim_y          = h.tiles_h;
    info->tile_count     = h.tiles_w * h.tiles_h;
    info->leaf_step      = h.leaf_step;
    info->leaves_w       = h.leaves_w;
    info->leaves_h       = h.leaves_h;
    info->leaf_offset    = h.leaf_off;
    info->summary_offset = h.summary_off;

    info->surface_dist.clear();
    for (uint32_t t = 0; t < info->tile_count; t++) {
        uint8_t surface_class = data[(size_t)h.summary_off + (size_t)t * 3];
        info->surface_dist[surface_class]++;
    }

    return true;
}

// Null-terminated string in a fixed-width header field.
static std::string header_str(const uint8_t* p, size_t width) {
    size_t n = 0;
    while (n < width && p[n]) n++;
    return std::string((const char*)p, n);
}

static void read_records(const uint8_t* p, size_t count,
                         std::vector<T2Record>* out) {
    out->resize(count);
    for (size_t i = 0; i < count; i++) {
        (*out)[i].surface_class   = p[i * 3];
        (*out)[i].elevation       = p[i * 3 + 1];
        (*out)[i].texture_variant = p[i * 3 + 2];
    }
}

bool t2_read(const uint8_t* data, size_t size, T2Map* map) {
    T2Header h;
    if (!parse_header(data, size, &h)) return false;

    map->theater   = header_str(data + 0x04, 0x40 - 0x04);
    map->atlas_pic = header_str(data + 0x54, 0x60 - 0x54);
    map->tiles_w   = h.tiles_w;
    map->tiles_h   = h.tiles_h;
    map->leaves_w  = h.leaves_w;
    map->leaves_h  = h.leaves_h;
    map->leaf_step = h.leaf_step;

    map->header.assign(data, data + h.leaf_off);
    read_records(data + h.leaf_off, (size_t)h.leaves_w * h.leaves_h,
                 &map->leaves);
    read_records(data + h.summary_off, (size_t)h.tiles_w * h.tiles_h,
                 &map->summaries);

    return true;
}

} // namespace fx
