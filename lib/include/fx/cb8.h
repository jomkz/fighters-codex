#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace fx {

struct Cb8Info {
    uint32_t width;
    uint32_t height;
    uint32_t frame_count;
    uint32_t samples_per_frame;  // sync counter ticks per frame (400)
    uint32_t audio_sync_rate;    // sync counter ticks per second (6000 = 400 Ã— 15 fps)
};

bool cb8_info(const uint8_t* data, size_t size, Cb8Info* out);

struct Cb8Decoder;
Cb8Decoder* cb8_open(const uint8_t* data, size_t size);
void        cb8_close(Cb8Decoder* dec);

// Returns palette index bytes (width Ã— height), row-major, top-to-bottom.
// Decodes incrementally; seeking backward resets canvas to frame 0.
// Returns empty on error or out-of-range frame_idx.
std::vector<uint8_t> cb8_decode_frame(Cb8Decoder* dec, uint32_t frame_idx);

} // namespace fx
