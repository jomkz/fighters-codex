#include "ft/sms.h"
#include <cstring>

namespace ft {

static uint32_t r32(const uint8_t* p) {
    return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

std::vector<SmsSymbol> sms_parse(const uint8_t* data, size_t size) {
    if (size < 4) return {};

    uint32_t count = r32(data);
    if (count == 0) return {};

    size_t rec_end = 4 + (size_t)count * 8;
    if (rec_end > size) return {};

    const uint8_t* strtab = data + rec_end;
    size_t strtab_size = size - rec_end;

    std::vector<SmsSymbol> syms;
    syms.reserve(count);

    for (uint32_t i = 0; i < count; i++) {
        const uint8_t* rec = data + 4 + i * 8;
        uint32_t str_off = r32(rec);
        uint32_t va      = r32(rec + 4);

        if (str_off >= strtab_size) continue;

        const char* s = (const char*)(strtab + str_off);
        size_t max_len = strtab_size - str_off;
        size_t len = strnlen(s, max_len);

        syms.push_back({ va, std::string(s, len) });
    }

    return syms;
}

} // namespace ft
