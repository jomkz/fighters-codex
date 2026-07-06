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

static void w32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

// Write a picture name into its fixed 22-byte slot: content, then a NUL
// when shorter than the slot (mirrors read_asciiz); tail bytes carry over.
static bool wr_pic(uint8_t* slot, const std::string& s) {
    if (s.size() > 22) return false;
    memcpy(slot, s.data(), s.size());
    if (s.size() < 22) slot[s.size()] = 0;
    return true;
}

std::vector<uint8_t> lay_repack(const uint8_t* orig, size_t orig_size,
                                const LayFile& lay) {
    CodeSection cs = pe_code_section(orig, orig_size);
    if (!cs.data || cs.size < 0x78) return {};

    // Walk the ORIGINAL layer array for the slot offsets and sentinel
    // pattern; the edited struct must keep the same shape.
    size_t arr_off = pe_va_to_offset(cs, u32le(cs.data + 0x74));
    if (arr_off == (size_t)-1) return {};
    const size_t LAYER_SZ = 0x160;
    std::vector<size_t> slots;
    std::vector<uint8_t> sentinels;
    for (size_t n = 0;; ++n) {
        size_t off = arr_off + n * LAYER_SZ;
        if (off + LAYER_SZ > cs.size) break;
        slots.push_back(off);
        sentinels.push_back((uint8_t)(cs.data[off] & 0x01));
        if (cs.data[off] & 0x01) break;
    }
    if (lay.layers.size() != slots.size()) return {};
    for (size_t i = 0; i < slots.size(); ++i)
        if ((lay.layers[i].flags & 0x01) != sentinels[i]) return {};

    // The structural VAs anchor tables that cannot be relocated by a field
    // rewrite — reject edits instead of writing a header that lies.
    if (lay.layer_array_va != u32le(cs.data + 0x74) ||
        lay.colour_entry_table_va != u32le(cs.data + 0x6C) ||
        lay.palette_buffer_va != u32le(cs.data + 0x70)) return {};

    std::vector<uint8_t> out(orig, orig + orig_size);
    uint8_t* ocs = out.data() + (cs.data - orig);

    // Header fields (offsets mirror lay_parse); the sky/below band tables
    // stay writable — they select which LAYER each altitude band uses.
    w32(ocs + 0x14, lay.sky_angle_scale);
    for (int i = 0; i < 10; ++i) w32(ocs + 0x18 + i * 4, lay.sky_layer_va[i]);
    w32(ocs + 0x40, lay.below_angle_scale);
    for (int i = 0; i < 10; ++i) w32(ocs + 0x44 + i * 4, lay.below_layer_va[i]);

    for (size_t i = 0; i < slots.size(); ++i) {
        const LayLayer& L = lay.layers[i];
        uint8_t* e = ocs + slots[i];
        e[0x00] = L.flags;
        w32(e + 0x02, (uint32_t)L.sel_alt_min);
        w32(e + 0x06, (uint32_t)L.sel_alt_max);
        w32(e + 0x0A, (uint32_t)L.alt_min);
        w32(e + 0x0E, (uint32_t)L.alt_max);
        w32(e + 0x12, (uint32_t)L.fog_alt_low);
        w32(e + 0x16, (uint32_t)L.vis_lo);
        w32(e + 0x1A, (uint32_t)L.fog_alt_high);
        w32(e + 0x1E, (uint32_t)L.vis_hi);
        w32(e + 0x22, (uint32_t)L.extinction_param);
        w32(e + 0x26, (uint32_t)L.gradient_alt_start);
        w32(e + 0x2A, (uint32_t)L.gradient_val_start);
        w32(e + 0x2E, (uint32_t)L.gradient_alt_end);
        w32(e + 0x32, (uint32_t)L.gradient_val_end);
        e[0x36] = L.base_rgb[0];
        e[0x37] = L.base_rgb[1];
        e[0x38] = L.base_rgb[2];
        for (int j = 0; j < 31; ++j) {
            e[0x3E + j * 3] = L.zenith_grad[j].r;
            e[0x3F + j * 3] = L.zenith_grad[j].g;
            e[0x40 + j * 3] = L.zenith_grad[j].b;
        }
        for (int j = 0; j < 32; ++j) {
            e[0x9B + j * 3] = L.horizon_grad[j].r;
            e[0x9C + j * 3] = L.horizon_grad[j].g;
            e[0x9D + j * 3] = L.horizon_grad[j].b;
        }
        e[0xFB] = L.horizon_base_rgb[0];
        e[0xFC] = L.horizon_base_rgb[1];
        e[0xFD] = L.horizon_base_rgb[2];
        w32(e + 0xFE, L.fog_density);
        if (!wr_pic(e + 0x102, L.cloud_pic)) return {};
        if (!wr_pic(e + 0x118, L.sky_pic)) return {};
        e[0x14E] = L.visibility;
    }
    return out;
}

} // namespace fx
