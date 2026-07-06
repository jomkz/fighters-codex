#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

// RAW screenshot codec.
//
// File layout (confirmed against captures at 1024x768, 800x600, 640x480,
// and 320x200 — #96):
//   0    6   Magic: "mhwanh"
//   6    2   Constant 00 04 (identical at every resolution)
//   8    2   Width  (u16 big-endian)
//  10    2   Height (u16 big-endian)
//  12    2   Constant 01 00
//  14   18   Null padding
//  32  768   Embedded palette: 256 x RGB8 triplets (8-bit, 0-255)
// 800  w*h   Pixel indices, row-major
//
// Width and height are stored big-endian despite the game executable being
// little-endian. Palette values are full 8-bit (not the 6-bit VGA format
// used by .PAL/.PIC).

namespace fx {

struct RawInfo {
    uint32_t width;
    uint32_t height;
};

// Parse header. Returns false if data is too short or magic is wrong.
bool raw_info(const uint8_t* data, size_t size, RawInfo* info);

// Decode RAW to a width*height*4 RGBA buffer.
// Returns empty vector on error.
std::vector<uint8_t> raw_decode(const uint8_t* data, size_t size);

// Encode RGBA to a RAW screenshot (#96): the palette is built from the
// image's distinct colours in first-seen order (alpha ignored — screenshots
// are opaque). Returns empty when the image needs more than 256 colours.
std::vector<uint8_t> raw_encode(const uint8_t* rgba, int w, int h);

// Byte-identical structural repack: re-serialize the parsed header fields
// and copy palette + pixels through. A non-empty result is always
// byte-identical to the input; unaccounted trailing bytes fail.
std::vector<uint8_t> raw_repack(const uint8_t* data, size_t size);

} // namespace fx
