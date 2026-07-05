#include "fx/fbc.h"

namespace fx {

static const uint64_t VDO_HEADER_SIZE = 816; // 48-byte fields + 768-byte palette

std::vector<uint32_t> fbc_read(const uint8_t* data, size_t size, bool* ok) {
    if (size % 4 != 0) {
        if (ok) *ok = false;
        return {};
    }
    if (ok) *ok = true;
    std::vector<uint32_t> sizes(size / 4);
    for (size_t i = 0; i < sizes.size(); i++) {
        const uint8_t* p = data + i * 4;
        sizes[i] = (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) |
                              ((uint32_t)p[3] << 24));
    }
    return sizes;
}

std::vector<uint8_t> fbc_write(const std::vector<uint32_t>& frame_sizes) {
    std::vector<uint8_t> out(frame_sizes.size() * 4);
    for (size_t i = 0; i < frame_sizes.size(); i++) {
        uint32_t v = frame_sizes[i];
        out[i * 4 + 0] = (uint8_t)v;
        out[i * 4 + 1] = (uint8_t)(v >> 8);
        out[i * 4 + 2] = (uint8_t)(v >> 16);
        out[i * 4 + 3] = (uint8_t)(v >> 24);
    }
    return out;
}

uint64_t fbc_frame_offset(const std::vector<uint32_t>& frame_sizes, size_t n) {
    uint64_t off = VDO_HEADER_SIZE;
    if (n > frame_sizes.size()) n = frame_sizes.size();
    for (size_t i = 0; i < n; i++) off += frame_sizes[i];
    return off;
}

} // namespace fx
