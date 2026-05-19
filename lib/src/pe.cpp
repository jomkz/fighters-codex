#include "ft/pe.h"

namespace ft {

static uint16_t u16le(const uint8_t* p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t u32le(const uint8_t* p) {
    return (uint32_t)(p[0] | ((uint32_t)p[1] << 8) |
                      ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

CodeSection pe_code_section(const uint8_t* data, size_t size) {
    if (size < 0x40 || data[0] != 'M' || data[1] != 'Z')
        return {nullptr, 0, 0};
    uint32_t pe_off = u32le(data + 0x3C);
    if (pe_off + 24 + 2 > size)
        return {nullptr, 0, 0};
    const uint8_t* pe = data + pe_off;
    if (pe[0] != 'P' || pe[1] != 'L' || pe[2] != 0 || pe[3] != 0)
        return {nullptr, 0, 0};
    uint16_t num_sec    = u16le(pe + 6);
    uint16_t opt_hdr_sz = u16le(pe + 20);
    uint32_t sec_table  = pe_off + 24 + opt_hdr_sz;
    for (uint16_t i = 0; i < num_sec; ++i) {
        uint32_t sec_off = sec_table + (uint32_t)i * 40;
        if (sec_off + 40 > size) break;
        const uint8_t* sec = data + sec_off;
        uint32_t virt_addr = u32le(sec + 12);  // section VirtualAddress
        uint32_t raw_sz    = u32le(sec + 16);
        uint32_t raw_ptr   = u32le(sec + 20);
        if (raw_sz > 0 && raw_ptr + raw_sz <= size)
            return {data + raw_ptr, raw_sz, virt_addr};
    }
    return {nullptr, 0, 0};
}

} // namespace ft
