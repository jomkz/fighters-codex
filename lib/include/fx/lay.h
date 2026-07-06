#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fx {

struct LayGrad {
    uint8_t r, g, b;
};

struct LayLayer {
    uint8_t  flags;            // bit0=end sentinel, bit1=brightness gradient enabled
    int32_t  sel_alt_min;
    int32_t  sel_alt_max;
    int32_t  alt_min;
    int32_t  alt_max;
    int32_t  fog_alt_low;
    int32_t  vis_lo;
    int32_t  fog_alt_high;
    int32_t  vis_hi;
    int32_t  extinction_param;
    int32_t  gradient_alt_start;
    int32_t  gradient_val_start;
    int32_t  gradient_alt_end;
    int32_t  gradient_val_end;
    uint8_t  base_rgb[3];
    LayGrad  zenith_grad[31];   // +0x3E: zenithâ†’horizon, 31 RGB entries
    LayGrad  horizon_grad[32];  // +0x9B: horizon downward, 32 RGB entries
    uint8_t  horizon_base_rgb[3];
    uint32_t fog_density;
    std::string cloud_pic;
    std::string sky_pic;
    uint8_t  visibility;
};

struct LayFile {
    bool     valid = false;

    // Header fields
    uint32_t sky_angle_scale;
    uint32_t below_angle_scale;
    uint32_t sky_layer_va[10];    // raw VAs; use as indices into layers[]
    uint32_t below_layer_va[10];
    uint32_t colour_entry_table_va;
    uint32_t palette_buffer_va;
    uint32_t layer_array_va;

    std::vector<LayLayer> layers;
};

LayFile lay_parse(const uint8_t* data, size_t size);

// Rebuild a LAY DLL around edited header fields and layers (#99).
// `lay.layers` must match the original layer count, and each layer's
// end-sentinel bit (flags bit 0) must match the original's — moving the
// sentinel would change the array length the engine walks. cloud_pic and
// sky_pic fit their fixed 22-byte slots (a NUL is kept when shorter than
// 22). The structural VAs (layer_array_va, colour_entry_table_va,
// palette_buffer_va) must match the original — their tables cannot be
// relocated; the sky/below band tables stay editable. Bytes the parser
// does not model — PE headers, colour tables, reserved layer regions —
// carry over verbatim, so an unedited parse→repack is byte-identical.
// Returns empty on a count/sentinel/VA mismatch or oversized picture
// names.
std::vector<uint8_t> lay_repack(const uint8_t* orig, size_t orig_size,
                                const LayFile& lay);

} // namespace fx
