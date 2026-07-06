#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

// Synthetic MZ/LE ("PL\0\0") image builder shared by test_pe.cpp and
// test_fnt.cpp. Lays out one CODE section exactly as pe_code_section
// expects: e_lfanew at 0x3C, NumberOfSections at pe+6, SizeOfOptionalHeader
// at pe+20, 40-byte section entries from pe+24. Mirrors
// fuzz/corpus/fuzz_pe/seed-le-code.bin.
namespace fx_test {

inline void put16(std::vector<uint8_t>& b, size_t off, uint16_t v) {
    b[off] = (uint8_t)v; b[off + 1] = (uint8_t)(v >> 8);
}
inline void put32(std::vector<uint8_t>& b, size_t off, uint32_t v) {
    b[off] = (uint8_t)v;             b[off + 1] = (uint8_t)(v >> 8);
    b[off + 2] = (uint8_t)(v >> 16); b[off + 3] = (uint8_t)(v >> 24);
}

// The CODE section is mapped at VMA 0x1000.
inline std::vector<uint8_t> make_le(const std::vector<uint8_t>& payload) {
    const uint32_t pe_off = 0x40;
    const uint32_t raw_ptr = pe_off + 24 + 40;
    std::vector<uint8_t> b(raw_ptr, 0);
    b[0] = 'M'; b[1] = 'Z';
    put32(b, 0x3C, pe_off);
    b[pe_off] = 'P'; b[pe_off + 1] = 'L';
    put16(b, pe_off + 6, 1);                    // NumberOfSections
    put16(b, pe_off + 20, 0);                   // SizeOfOptionalHeader
    const uint32_t sec = pe_off + 24;
    memcpy(&b[sec], "CODE", 4);
    put32(b, sec + 8, (uint32_t)payload.size());   // VirtualSize
    put32(b, sec + 12, 0x1000);                    // VirtualAddress
    put32(b, sec + 16, (uint32_t)payload.size());  // SizeOfRawData
    put32(b, sec + 20, raw_ptr);                   // PointerToRawData
    b.insert(b.end(), payload.begin(), payload.end());
    return b;
}

}  // namespace fx_test
