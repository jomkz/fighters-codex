#include "ft/raw.h"
#include <cstring>

namespace ft {

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

} // namespace ft
