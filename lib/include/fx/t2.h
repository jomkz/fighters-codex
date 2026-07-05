#pragma once
#include <cstddef>
#include <cstdint>
#include <map>

// T2 terrain map parser.
//
// Engine field map (T_Load @0x4C5D70 relocates the two array offsets;
// T_GetLeaf @0x4C6040 indexes both arrays — see docs/fa/formats/T2.md):
//   0x00   4   Magic "BIT2"
//   0x04   *   Null-terminated theater display name
//   0x54   *   Null-terminated associated .PIC filename
//   0x64   4   grid rows (legacy duplicate of tiles_h)
//   0x79   4   leaf_step — leaves per tile side (8 in all theaters)
//   0x7D   4   tiles_w   — tile grid width
//   0x81   4   tiles_h   — tile grid height
//   0x85   4   file offset of the tile-summary array (tiles_w*tiles_h x 3 B)
//   0x89   4   leaves_w  (= tiles_w * leaf_step)
//   0x8D   4   leaves_h  (= tiles_h * leaf_step)
//   0x91   4   file offset of the leaf array (leaves_w*leaves_h x 3 B) = 0x95
// Payload = two flat row-major arrays of 3-byte records: the leaf array at
// 0x95, then the per-tile summary array (the coarse-LOD representative
// T_GetLeaf returns when the requested step exceeds leaf_step).

namespace fx {

struct T2Info {
    uint32_t dim_x;           // tile grid width  (engine tiles_w)
    uint32_t dim_y;           // tile grid height (engine tiles_h)
    uint32_t tile_count;      // dim_x * dim_y
    uint32_t leaf_step;       // leaves per tile side (8)
    uint32_t leaves_w;        // leaf grid width
    uint32_t leaves_h;        // leaf grid height
    uint32_t leaf_offset;     // leaf array file offset (0x95)
    uint32_t summary_offset;  // tile-summary array file offset

    // Surface class distribution over the per-tile summary records:
    // key = surface_class byte, value = tile count
    std::map<uint8_t, uint32_t> surface_dist;
};

// Parse T2 header and build the per-tile surface class distribution.
// Returns false if magic doesn't match or the field map is inconsistent
// with the file size.
bool t2_info(const uint8_t* data, size_t size, T2Info* info);

} // namespace fx
