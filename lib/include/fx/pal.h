#pragma once
#include <cstddef>
#include <cstdint>

// VGA 6-bit palette (256 colors x 3 bytes = 768 bytes raw).

namespace fx {

// Widen a 6-bit VGA channel (0-63) to 8-bit by bit replication — the standard
// VGA DAC expansion, so 63 maps to full-range 255. The single source of truth
// for palette widening across fx_lib and fx_render (#369).
inline uint8_t pal_widen6(uint8_t c6) {
    return (uint8_t)((c6 << 2) | (c6 >> 4));
}

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
