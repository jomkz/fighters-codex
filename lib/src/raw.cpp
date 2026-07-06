#include "fx/raw.h"
#include <cstring>
#include <map>

namespace fx {

static const int HEADER_SIZE  = 32;
static const int PALETTE_SIZE = 768;
static const int DATA_OFFSET  = HEADER_SIZE + PALETTE_SIZE; // 800

static uint16_t r16be(const uint8_t* d, int o) {
    return (uint16_t)((d[o] << 8) | d[o+1]);
}

bool raw_info(const uint8_t* data, size_t size, RawInfo* info) {
    if (size < (size_t)DATA_OFFSET) return false;
    if (memcmp(data, "mhwanh", 6) != 0) return false;
    uint32_t w = r16be(data, 8);
    uint32_t h = r16be(data, 10);
    if (w == 0 || h == 0) return false;
    if (size < (size_t)(DATA_OFFSET + w * h)) return false;
    info->width  = w;
    info->height = h;
    return true;
}

std::vector<uint8_t> raw_decode(const uint8_t* data, size_t size) {
    RawInfo info;
    if (!raw_info(data, size, &info)) return {};

    int w = (int)info.width;
    int h = (int)info.height;
    const uint8_t* pal = data + HEADER_SIZE;       // 256 x RGB8
    const uint8_t* pix = data + DATA_OFFSET;

    std::vector<uint8_t> rgba((size_t)w * h * 4);
    for (int i = 0; i < w * h; i++) {
        uint8_t idx = pix[i];
        rgba[i*4+0] = pal[idx*3+0];
        rgba[i*4+1] = pal[idx*3+1];
        rgba[i*4+2] = pal[idx*3+2];
        rgba[i*4+3] = 255;
    }
    return rgba;
}

static void w16be(uint8_t* d, int o, uint16_t v) {
    d[o]     = (uint8_t)(v >> 8);
    d[o + 1] = (uint8_t)v;
}

std::vector<uint8_t> raw_encode(const uint8_t* rgba, int w, int h) {
    if (!rgba || w <= 0 || h <= 0 || w > 0xFFFF || h > 0xFFFF) return {};

    std::vector<uint8_t> out((size_t)DATA_OFFSET + (size_t)w * h, 0);
    memcpy(out.data(), "mhwanh", 6);
    out[6] = 0x00;             // constant across every observed resolution
    out[7] = 0x04;
    w16be(out.data(), 8, (uint16_t)w);
    w16be(out.data(), 10, (uint16_t)h);
    out[12] = 0x01;            // constant
    out[13] = 0x00;

    // Exact-colour palette in first-seen order; 8-bit RGB, alpha ignored.
    uint8_t* pal = out.data() + HEADER_SIZE;
    uint8_t* pix = out.data() + DATA_OFFSET;
    std::map<uint32_t, uint8_t> seen;
    for (int i = 0; i < w * h; i++) {
        const uint32_t key = (uint32_t)rgba[i*4] << 16 |
                             (uint32_t)rgba[i*4+1] << 8 | rgba[i*4+2];
        auto it = seen.find(key);
        if (it == seen.end()) {
            if (seen.size() >= 256) return {};
            const uint8_t id = (uint8_t)seen.size();
            it = seen.emplace(key, id).first;
            pal[id*3+0] = rgba[i*4+0];
            pal[id*3+1] = rgba[i*4+1];
            pal[id*3+2] = rgba[i*4+2];
        }
        pix[i] = it->second;
    }
    return out;
}

std::vector<uint8_t> raw_repack(const uint8_t* data, size_t size) {
    RawInfo info;
    if (!raw_info(data, size, &info)) return {};
    const size_t total = (size_t)DATA_OFFSET + (size_t)info.width * info.height;
    if (size != total) return {};  // trailing bytes = unknown structure

    std::vector<uint8_t> out(total, 0);
    memcpy(out.data(), "mhwanh", 6);
    out[6] = data[6];              // constant field carried verbatim
    out[7] = data[7];
    w16be(out.data(), 8, (uint16_t)info.width);   // re-serialized (proving them)
    w16be(out.data(), 10, (uint16_t)info.height);
    memcpy(out.data() + 12, data + 12, HEADER_SIZE - 12);  // constant + padding
    memcpy(out.data() + HEADER_SIZE, data + HEADER_SIZE,
           PALETTE_SIZE + (size_t)info.width * info.height);
    return out;
}

} // namespace fx
