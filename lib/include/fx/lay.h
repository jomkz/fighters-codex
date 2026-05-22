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

} // namespace fx
