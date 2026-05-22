#include "fx/lay.h"
#include "fx/pe.h"
#include <cstring>

namespace fx {

static int32_t s32le(const uint8_t* p) {
    return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                     ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}
static uint32_t u32le(const uint8_t* p) {
    return (uint32_t)(p[0] | ((uint32_t)p[1] << 8) |
                      ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

static std::string read_asciiz(const uint8_t* p, size_t max) {
    std::string s;
    for (size_t i = 0; i < max && p[i]; ++i) s += (char)p[i];
    return s;
}

LayFile lay_parse(const uint8_t* data, size_t size) {
    LayFile result{};
    CodeSection cs = pe_code_section(data, size);
    if (!cs.data || cs.size < 0x78) return result;

    // DLL data header: first 0x78 bytes of CODE section (VA 0x1000)
    const uint8_t* hdr = cs.data;

    result.sky_angle_scale   = u32le(hdr + 0x14);
    for (int i = 0; i < 10; ++i)
        result.sky_layer_va[i]   = u32le(hdr + 0x18 + i * 4);
    result.below_angle_scale  = u32le(hdr + 0x40);
    for (int i = 0; i < 10; ++i)
        result.below_layer_va[i] = u32le(hdr + 0x44 + i * 4);
    result.colour_entry_table_va = u32le(hdr + 0x6C);
    result.palette_buffer_va     = u32le(hdr + 0x70);
    result.layer_array_va        = u32le(hdr + 0x74);

    // Walk LAYER array
    size_t arr_off = pe_va_to_offset(cs, result.layer_array_va);
    if (arr_off == (size_t)-1) return result;

    const size_t LAYER_SZ = 0x160;
    for (size_t n = 0; ; ++n) {
        size_t off = arr_off + n * LAYER_SZ;
        if (off + LAYER_SZ > cs.size) break;
        const uint8_t* e = cs.data + off;

        LayLayer lay{};
        lay.flags            = e[0x00];
        lay.sel_alt_min      = s32le(e + 0x02);
        lay.sel_alt_max      = s32le(e + 0x06);
        lay.alt_min          = s32le(e + 0x0A);
        lay.alt_max          = s32le(e + 0x0E);
        lay.fog_alt_low      = s32le(e + 0x12);
        lay.vis_lo           = s32le(e + 0x16);
        lay.fog_alt_high     = s32le(e + 0x1A);
        lay.vis_hi           = s32le(e + 0x1E);
        lay.extinction_param = s32le(e + 0x22);
        lay.gradient_alt_start = s32le(e + 0x26);
        lay.gradient_val_start = s32le(e + 0x2A);
        lay.gradient_alt_end   = s32le(e + 0x2E);
        lay.gradient_val_end   = s32le(e + 0x32);
        lay.base_rgb[0] = e[0x36];
        lay.base_rgb[1] = e[0x37];
        lay.base_rgb[2] = e[0x38];
        for (int i = 0; i < 31; ++i) {
            lay.zenith_grad[i]  = {e[0x3E + i*3], e[0x3F + i*3], e[0x40 + i*3]};
        }
        for (int i = 0; i < 32; ++i) {
            lay.horizon_grad[i] = {e[0x9B + i*3], e[0x9C + i*3], e[0x9D + i*3]};
        }
        lay.horizon_base_rgb[0] = e[0xFB];
        lay.horizon_base_rgb[1] = e[0xFC];
        lay.horizon_base_rgb[2] = e[0xFD];
        lay.fog_density  = u32le(e + 0xFE);
        lay.cloud_pic    = read_asciiz(e + 0x102, 22);
        lay.sky_pic      = read_asciiz(e + 0x118, 22);
        lay.visibility   = e[0x14E];

        result.layers.push_back(lay);

        if (lay.flags & 0x01) break;  // end-of-array sentinel
    }

    result.valid = true;
    return result;
}

} // namespace fx
