#pragma once
#include "pal.h"
#include <cstddef>
#include <cstdint>
#include <vector>

// PIC image codec. Three sub-formats selected by the first uint16:
//   0x0000 -- Dense (texture): sequential row-major pixels + row-head offset table
//   0x0001 -- Sparse (image): 10-byte span records, mostly-transparent overlays
//   0xD8FF -- JPEG: entire PIC data is a standard JPEG file
//
// Palette index 0xFF = transparent (alpha=0 in RGBA output).

namespace fx {

struct PicInfo {
    uint16_t format;          // 0, 1, or 0xD8FF
    uint32_t width;
    uint32_t height;
    uint32_t pixels_offset;
    uint32_t pixels_size;
    uint32_t palette_offset;
    uint32_t palette_size;
    uint32_t spans_offset;
    uint32_t spans_size;
    uint32_t rowheads_offset;
    uint32_t rowheads_size;
};

// Parse PIC header. Returns false if data is too short.
bool pic_info(const uint8_t* data, size_t size, PicInfo* info);

// Decode PIC to a width*height*4 RGBA buffer.
// sys_pal: system PALETTE.PAL; inline palette fragment overlays on top.
// Pass nullptr for sys_pal to use greyscale.
// Returns empty vector on error.
std::vector<uint8_t> pic_decode(const uint8_t* data, size_t size,
                                 const Palette* sys_pal);

// Encode RGBA to a dense PIC (format=0) with a full inline palette.
// Pixels with alpha < 128 are written as index 0xFF (transparent).
// Quantization: nearest-neighbor Euclidean RGB distance across 255 colors.
// Returns empty vector on error.
std::vector<uint8_t> pic_encode(const uint8_t* rgba, int w, int h,
                                 const Palette& pal);

} // namespace fx
