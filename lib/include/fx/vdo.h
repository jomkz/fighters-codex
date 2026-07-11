#pragma once
#include "pal.h"
#include <cstddef>
#include <cstdint>
#include <vector>

// VDO — RATVID streaming video (mission-briefing FMV). Engine-traced codec
// (#137-#140): a per-pixel delta scheme. Each frame carries a run-length
// coded *mask* stream (one byte per 8-pixel group; a set bit = the pixel
// changed) and a *source* stream of just the changed pixels; a decoder walks
// the groups copying masked pixels from the source and keeping the rest from
// the previous frame. Frame boundaries come from the paired .FBC index; the
// 256-colour VGA palette is in the .VDO header. Read-only (no encoder in the
// engine). See docs/fa/formats/VDO.md.

namespace fx {

struct VdoInfo {
    uint32_t width;
    uint32_t height;
    uint32_t frame_count;   // 0 if no FBC supplied
    uint32_t fps;
    uint32_t audio_hz;      // paired .11K sample rate (mono 8-bit PCM)
};

// Parse the 816-byte header. Validates the "RATVID" magic and plausible
// dimensions. frame_count is filled only when fbc data is provided to
// vdo_open; vdo_info leaves it 0.
bool vdo_info(const uint8_t* data, size_t size, VdoInfo* out);

struct VdoDecoder;

// Open a decoder over the .VDO bytes and its paired .FBC frame-size index.
// Returns null if the header is invalid or the FBC does not match. The
// decoder owns copies of nothing beyond what it needs; keep `vdo`/`fbc`
// alive for its lifetime.
VdoDecoder* vdo_open(const uint8_t* vdo, size_t vdo_size,
                     const uint8_t* fbc, size_t fbc_size);
void        vdo_close(VdoDecoder* dec);

// Header palette, widened 6->8 bit (pal_load semantics).
bool vdo_palette(const VdoDecoder* dec, Palette* out);
uint32_t vdo_frame_count(const VdoDecoder* dec);
uint32_t vdo_width(const VdoDecoder* dec);
uint32_t vdo_height(const VdoDecoder* dec);

// Decode up to and including frame `idx`, returning that frame's palette
// indices (width*height, row-major). Frames are inter-coded, so decoding is
// sequential: requesting a frame earlier than the decoder's cursor rewinds to
// frame 0 and replays. Returns empty on error (bad frame data / out of range).
std::vector<uint8_t> vdo_decode_frame(VdoDecoder* dec, uint32_t idx);

// Same, converted to RGBA8 through the header palette.
std::vector<uint8_t> vdo_decode_frame_rgba(VdoDecoder* dec, uint32_t idx);

} // namespace fx
