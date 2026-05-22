#include "fx/pal.h"
#include <algorithm>

namespace fx {

static inline uint8_t vga_to_8(uint8_t v) {
    return (uint8_t)((v << 2) | (v >> 6));
}

Palette pal_load(const uint8_t* data, size_t size) {
    Palette pal = {};
    if (!data || size < 3) {
        for (int i = 0; i < 256; i++)
            pal.r[i] = pal.g[i] = pal.b[i] = (uint8_t)i; // greyscale fallback
        return pal;
    }
    int count = (int)std::min(size / 3, (size_t)256);
    for (int i = 0; i < count; i++) {
        pal.r[i] = vga_to_8(data[i * 3 + 0]);
        pal.g[i] = vga_to_8(data[i * 3 + 1]);
        pal.b[i] = vga_to_8(data[i * 3 + 2]);
    }
    return pal;
}

void pal_save(const Palette& pal, uint8_t out[768]) {
    for (int i = 0; i < 256; i++) {
        out[i * 3 + 0] = pal.r[i] >> 2;
        out[i * 3 + 1] = pal.g[i] >> 2;
        out[i * 3 + 2] = pal.b[i] >> 2;
    }
}

} // namespace fx
