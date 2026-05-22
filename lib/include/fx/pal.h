#pragma once
#include <cstddef>
#include <cstdint>

// VGA 6-bit palette (256 colors x 3 bytes = 768 bytes raw).
// Each channel is 0-63; scale to 8-bit with rotate_left(2): (c << 2) | (c >> 6).

namespace fx {

struct Palette {
    uint8_t r[256];
    uint8_t g[256];
    uint8_t b[256];
};

// Load palette from raw PAL bytes (768 bytes, VGA 6-bit).
// If data is null or size < 3, returns a greyscale palette.
Palette pal_load(const uint8_t* data, size_t size);

// Serialize palette back to raw VGA 6-bit bytes (768 bytes written to out).
void pal_save(const Palette& pal, uint8_t out[768]);

} // namespace fx
