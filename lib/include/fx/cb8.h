#pragma once
#include "pal.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// CB8 FMV container. Engine-traced model (#95): every video frame (MRFI,
// kind 0 / submode 5) is a self-contained vector-quantized keyframe carrying
// its own 768-byte 6-bit palette, a detail codebook of 2x2-pixel entries
// (optionally split into two row bands at a switch row), a single codebook
// expanded 2x2 -> 4x4, a mode bitmap (u32-LE words consumed MSB-first, one
// bit per 4x4 cell), and an index stream: bit 0 -> one single-book index,
// bit 1 -> four detail-book indices (TL, TR, BL, BR). There is no inter
// frame for the 8-bit path. Audio is raw 8-bit 11025 Hz PCM in MRFA chunks;
// VooM is the frame index. See docs/fa/formats/CB8.md.

namespace fx {

struct Cb8Info {
    uint32_t width;
    uint32_t height;
    uint32_t frame_count;
    uint32_t samples_per_frame;  // sync counter ticks per frame
    uint32_t audio_sync_rate;    // sync counter ticks per second
};

bool cb8_info(const uint8_t* data, size_t size, Cb8Info* out);

struct Cb8Decoder;
Cb8Decoder* cb8_open(const uint8_t* data, size_t size);
void        cb8_close(Cb8Decoder* dec);

// Palette-index frame (width x height, row-major). Frames are independent —
// any frame decodes directly. Empty on error. Single-book cells expand by
// plain pixel doubling (the engine's ExpandDB; its EDB dither variant is a
// display-time option documented in the spec, not part of the stream).
std::vector<uint8_t> cb8_decode_frame(Cb8Decoder* dec, uint32_t frame_idx);

// The frame's embedded palette, widened 6->8 bit like pal_load.
bool cb8_frame_palette(Cb8Decoder* dec, uint32_t frame_idx, Palette* out);

// RGBA8 decode through the frame's embedded palette.
std::vector<uint8_t> cb8_decode_frame_rgba(Cb8Decoder* dec, uint32_t frame_idx);

// One re-encodable frame: indices plus the 6-bit palette to embed.
struct Cb8Frame {
    std::vector<uint8_t> indices;         // width * height
    std::array<uint8_t, 768> palette6{};  // 6-bit VGA RGB
};

// Rebuild a CB8 around new video frames. The DRBC header, every audio chunk,
// the stream order, and the VooM timing fields are carried from `orig`
// verbatim; each MRFI is re-encoded (single cells where the 4x4 is a 2x2
// doubling, detail cells otherwise; codebooks rebuilt per frame with an
// automatic band split). frames.size() must equal the original frame count.
// Returns empty if a frame cannot be represented (more than 256 distinct
// codebook entries in a band after splitting).
std::vector<uint8_t> cb8_repack(const uint8_t* orig, size_t orig_size,
                                const std::vector<Cb8Frame>& frames);

} // namespace fx
