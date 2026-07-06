#include "fx/pic.h"
#include <algorithm>
#include <cstring>

// stb_image for JPEG decoding inside JPEG-format PICs
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace fx {

static const int HEADER_SIZE = 64;

static uint16_t r16(const uint8_t* d, int o) {
    return (uint16_t)(d[o] | (d[o+1] << 8));
}
static uint32_t r32(const uint8_t* d, int o) {
    return (uint32_t)(d[o] | (d[o+1]<<8) | (d[o+2]<<16) | (d[o+3]<<24));
}
static void w32(uint8_t* d, int o, uint32_t v) {
    d[o+0] = (uint8_t)(v);
    d[o+1] = (uint8_t)(v >>  8);
    d[o+2] = (uint8_t)(v >> 16);
    d[o+3] = (uint8_t)(v >> 24);
}

bool pic_info(const uint8_t* data, size_t size, PicInfo* info) {
    if (size < (size_t)HEADER_SIZE) return false;
    info->format          = r16(data,  0);
    info->width           = r32(data,  2);
    info->height          = r32(data,  6);
    info->pixels_offset   = r32(data, 10);
    info->pixels_size     = r32(data, 14);
    info->palette_offset  = r32(data, 18);
    info->palette_size    = r32(data, 22);
    info->spans_offset    = r32(data, 26);
    info->spans_size      = r32(data, 30);
    info->rowheads_offset = r32(data, 34);
    info->rowheads_size   = r32(data, 38);
    return true;
}

// Nearest-colour palette match (Euclidean RGB distance).
// Searches indices 0..254 only; index 255 is reserved for transparent.
static int find_closest(uint8_t r, uint8_t g, uint8_t b, const Palette& pal) {
    int best = 0;
    long best_dist = LONG_MAX;
    for (int i = 0; i < 255; i++) {
        long dr = (long)pal.r[i] - r;
        long dg = (long)pal.g[i] - g;
        long db = (long)pal.b[i] - b;
        long d  = dr*dr + dg*dg + db*db;
        if (d < best_dist) { best_dist = d; best = i; }
    }
    return best;
}

std::vector<uint8_t> pic_decode(const uint8_t* data, size_t size,
                                 const Palette* sys_pal) {
    if (size < (size_t)HEADER_SIZE) return {};

    uint16_t fmt = r16(data, 0);

    if (fmt == 0xD8FF) {
        int w = 0, h = 0, ch = 0;
        uint8_t* jpg = stbi_load_from_memory(data, (int)size, &w, &h, &ch, 4);
        if (!jpg) return {};
        std::vector<uint8_t> out(jpg, jpg + (size_t)w * h * 4);
        stbi_image_free(jpg);
        return out;
    }

    PicInfo info;
    if (!pic_info(data, size, &info)) return {};
    int w = (int)info.width;
    int h = (int)info.height;
    if (w <= 0 || h <= 0) return {};

    // Build effective palette: sys_pal base + inline fragment overlay
    Palette pal = sys_pal ? *sys_pal : Palette{};
    if (info.palette_size > 0 && info.palette_offset + info.palette_size <= size) {
        const uint8_t* p = data + info.palette_offset;
        int count = (int)std::min(info.palette_size / 3u, 256u);
        for (int i = 0; i < count; i++) {
            uint8_t rv = p[i*3+0], gv = p[i*3+1], bv = p[i*3+2];
            pal.r[i] = (uint8_t)((rv << 2) | (rv >> 6));
            pal.g[i] = (uint8_t)((gv << 2) | (gv >> 6));
            pal.b[i] = (uint8_t)((bv << 2) | (bv >> 6));
        }
    }

    std::vector<uint8_t> rgba((size_t)w * h * 4, 0);

    auto set_pixel = [&](int x, int y, uint8_t idx) {
        if (x < 0 || x >= w || y < 0 || y >= h) return;
        size_t off = (size_t)(y * w + x) * 4;
        if (idx == 0xFF) {
            rgba[off+0] = rgba[off+1] = rgba[off+2] = rgba[off+3] = 0;
        } else {
            rgba[off+0] = pal.r[idx];
            rgba[off+1] = pal.g[idx];
            rgba[off+2] = pal.b[idx];
            rgba[off+3] = 255;
        }
    };

    if (fmt == 0) {
        if (info.pixels_offset + (uint32_t)(w * h) > size) return {};
        const uint8_t* pix = data + info.pixels_offset;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                set_pixel(x, y, pix[y * w + x]);

    } else if (fmt == 1) {
        int span_count = (int)(info.spans_size / 10);
        for (int s = 0; s < span_count; s++) {
            size_t soff = info.spans_offset + (size_t)s * 10;
            if (soff + 10 > size) break;
            uint16_t row   = r16(data, (int)soff);
            uint16_t start = r16(data, (int)soff + 2);
            uint16_t end   = r16(data, (int)soff + 4);
            uint32_t idx   = r32(data, (int)soff + 6);
            if (row == 0xFFFF) break;
            for (int x = start; x <= (int)end; x++) {
                size_t poff = idx + (size_t)(x - start);
                if (poff >= size) break;
                set_pixel(x, (int)row, data[poff]);
            }
        }
    } else {
        return {};
    }

    return rgba;
}

std::vector<uint8_t> pic_repack(const uint8_t* data, size_t size) {
    if (!data || size < 2) return {};
    const uint16_t fmt = r16(data, 0);
    // JPEG PICs are the JPEG stream itself (0xFFD8 SOI read LE): the whole
    // file passes through.
    if (fmt == 0xD8FF) return std::vector<uint8_t>(data, data + size);
    if (size < (size_t)HEADER_SIZE) return {};

    PicInfo info;
    if (!pic_info(data, size, &info)) return {};
    if (fmt != 0 && fmt != 1) return {};

    std::vector<uint8_t> out(size, 0);
    std::vector<uint8_t> covered(size, 0);

    // Copy one header-described region through at its recorded offset,
    // refusing overlaps — a double-covered byte means the layout is not
    // what the header claims.
    auto place = [&](uint32_t off, uint32_t sz) -> bool {
        if (sz == 0) return true;
        if ((uint64_t)off + sz > size) return false;
        for (uint32_t i = 0; i < sz; i++) {
            if (covered[off + i]) return false;
            covered[off + i] = 1;
        }
        memcpy(out.data() + off, data + off, sz);
        return true;
    };

    // Header: re-serialize the parsed fields (proving them), carry the tail
    // at 42..63 verbatim (font_data_offset etc. — the re-static #54 gap).
    out[0] = (uint8_t)(info.format);
    out[1] = (uint8_t)(info.format >> 8);
    w32(out.data(),  2, info.width);
    w32(out.data(),  6, info.height);
    w32(out.data(), 10, info.pixels_offset);
    w32(out.data(), 14, info.pixels_size);
    w32(out.data(), 18, info.palette_offset);
    w32(out.data(), 22, info.palette_size);
    w32(out.data(), 26, info.spans_offset);
    w32(out.data(), 30, info.spans_size);
    w32(out.data(), 34, info.rowheads_offset);
    w32(out.data(), 38, info.rowheads_size);
    memcpy(out.data() + 42, data + 42, HEADER_SIZE - 42);
    std::fill(covered.begin(), covered.begin() + HEADER_SIZE, 1);

    // A real region lives past the header. Dense files carry a vestigial
    // spans_size with spans_offset == 0 — stale tool output the engine
    // never reads (font PICs even hold another image's value); it is
    // re-serialized verbatim in the header and never placed as a region.
    auto place_real = [&](uint32_t off, uint32_t sz) -> bool {
        if (sz == 0 || off < (uint32_t)HEADER_SIZE) return true;
        return place(off, sz);
    };
    if (!place_real(info.pixels_offset,   info.pixels_size))   return {};
    if (!place_real(info.palette_offset,  info.palette_size))  return {};
    if (!place_real(info.spans_offset,    info.spans_size))    return {};
    if (!place_real(info.rowheads_offset, info.rowheads_size)) return {};

    // Font PICs: the header-tail field at 0x2A points at a trailing block
    // (256 x 6-byte glyph records in every install file) running to end of
    // file, 16-aligned like the other late regions.
    const uint32_t font_off = r32(data, 42);
    if (font_off) {
        if (font_off < (uint32_t)HEADER_SIZE || font_off > size) return {};
        if (!place(font_off, (uint32_t)(size - font_off))) return {};
    }

    // The only bytes no region accounts for are short all-zero runs padding
    // the next region to a 16-byte boundary (sparse span tables and font
    // blocks in the install). Anything else is unknown structure — fail
    // rather than silently copy it.
    for (size_t i = 0; i < size;) {
        if (covered[i]) {
            i++;
            continue;
        }
        size_t j = i;
        while (j < size && !covered[j] && data[j] == 0) j++;
        const bool ends_at_aligned_region =
            j < size && covered[j] && (j % 16 == 0) && (j - i) < 16;
        if (!ends_at_aligned_region) return {};
        i = j;  // pad accepted; `out` is already zero there
    }
    return out;
}

std::vector<uint8_t> pic_encode(const uint8_t* rgba, int w, int h,
                                 const Palette& pal) {
    if (!rgba || w <= 0 || h <= 0) return {};

    std::vector<uint8_t> pixels((size_t)w * h);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            size_t si = (size_t)(y * w + x) * 4;
            uint8_t a = rgba[si + 3];
            if (a < 128) {
                pixels[y * w + x] = 0xFF;
            } else {
                pixels[y * w + x] = (uint8_t)find_closest(rgba[si], rgba[si+1], rgba[si+2], pal);
            }
        }
    }

    uint32_t pix_off = HEADER_SIZE;
    uint32_t pix_sz  = (uint32_t)(w * h);
    uint32_t pal_off = pix_off + pix_sz;
    uint32_t pal_sz  = 768;
    uint32_t rh_off  = pal_off + pal_sz;
    uint32_t rh_sz   = (uint32_t)(h * 4);

    std::vector<uint8_t> out(HEADER_SIZE + pix_sz + pal_sz + rh_sz, 0);

    w32(out.data(),  2, (uint32_t)w);
    w32(out.data(),  6, (uint32_t)h);
    w32(out.data(), 10, pix_off);
    w32(out.data(), 14, pix_sz);
    w32(out.data(), 18, pal_off);
    w32(out.data(), 22, pal_sz);
    w32(out.data(), 34, rh_off);
    w32(out.data(), 38, rh_sz);

    memcpy(out.data() + pix_off, pixels.data(), pix_sz);

    uint8_t* pd = out.data() + pal_off;
    for (int i = 0; i < 256; i++) {
        pd[i*3+0] = pal.r[i] >> 2;
        pd[i*3+1] = pal.g[i] >> 2;
        pd[i*3+2] = pal.b[i] >> 2;
    }

    for (int y = 0; y < h; y++) {
        uint32_t row_off = pix_off + (uint32_t)(y * w);
        w32(out.data(), (int)rh_off + y * 4, row_off);
    }

    return out;
}

} // namespace fx
