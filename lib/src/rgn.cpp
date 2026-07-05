#include "fx/rgn.h"
#include <cstring>

namespace fx {

static uint32_t r32(const uint8_t* p) {
    return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) |
                      ((uint32_t)p[3] << 24));
}
static void w32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v;         p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

bool rgn_read(const uint8_t* data, size_t size, RgnFile& out) {
    if (size < 4) return false;
    uint32_t count = r32(data);
    if (size != 4 + (uint64_t)count * 40) return false;

    out.records.assign(count, RgnRecord{});
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t* r = data + 4 + i * 40;
        memcpy(out.records[i].name, r, 4);
        out.records[i].vertex_count = r32(r + 4);
        for (int k = 0; k < 8; k++)
            out.records[i].xy[k] = r32(r + 8 + k * 4);
    }
    return true;
}

std::vector<uint8_t> rgn_write(const RgnFile& rgn) {
    std::vector<uint8_t> out(4 + rgn.records.size() * 40, 0);
    w32(out.data(), (uint32_t)rgn.records.size());
    for (size_t i = 0; i < rgn.records.size(); i++) {
        uint8_t* r = out.data() + 4 + i * 40;
        memcpy(r, rgn.records[i].name, 4);
        w32(r + 4, rgn.records[i].vertex_count);
        for (int k = 0; k < 8; k++)
            w32(r + 8 + k * 4, rgn.records[i].xy[k]);
    }
    return out;
}

std::string rgn_name(const RgnRecord& rec) {
    std::string s;
    for (int i = 0; i < 4 && rec.name[i]; i++) {
        uint8_t c = rec.name[i];
        s += (c >= 0x20 && c < 0x7F) ? (char)c : '.';
    }
    return s;
}

} // namespace fx
