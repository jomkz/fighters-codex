#include <catch2/catch_test_macros.hpp>
#include <fx/pe.h>
#include <cstring>
#include <vector>

using namespace fx;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void put16(std::vector<uint8_t>& b, size_t off, uint16_t v) {
    b[off] = (uint8_t)v; b[off + 1] = (uint8_t)(v >> 8);
}
static void put32(std::vector<uint8_t>& b, size_t off, uint32_t v) {
    b[off] = (uint8_t)v;         b[off + 1] = (uint8_t)(v >> 8);
    b[off + 2] = (uint8_t)(v >> 16); b[off + 3] = (uint8_t)(v >> 24);
}

// Minimal MZ/LE ("PL\0\0") image with one CODE section, laid out exactly as
// pe_code_section expects: e_lfanew at 0x3C, NumberOfSections at pe+6,
// SizeOfOptionalHeader at pe+20, 40-byte section entries from pe+24.
// Mirrors fuzz/corpus/fuzz_pe/seed-le-code.bin.
static std::vector<uint8_t> make_le(const std::vector<uint8_t>& payload) {
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

// ---------------------------------------------------------------------------
// pe_code_section
// ---------------------------------------------------------------------------

TEST_CASE("pe_code_section finds the CODE section in a minimal LE image") {
    std::vector<uint8_t> payload = { 0xB8, 0x01, 0x00, 0x00, 0x00, 0xC3 };
    auto img = make_le(payload);
    auto cs = pe_code_section(img.data(), img.size());
    REQUIRE(cs.data != nullptr);
    REQUIRE(cs.size == payload.size());
    REQUIRE(cs.vma == 0x1000);
    REQUIRE(memcmp(cs.data, payload.data(), payload.size()) == 0);
}

TEST_CASE("pe_code_section rejects non-MZ and undersized buffers") {
    auto img = make_le({ 0xC3 });
    img[0] = 'X';
    REQUIRE(pe_code_section(img.data(), img.size()).data == nullptr);
    img[0] = 'M';
    REQUIRE(pe_code_section(img.data(), 0x3F).data == nullptr);
}

TEST_CASE("pe_code_section rejects a wrapping e_lfanew") {
    // fuzz_pe finding: e_lfanew near UINT32_MAX wrapped the 32-bit header
    // bounds check and the PL signature was read out of bounds.
    auto img = make_le({ 0xC3 });
    put32(img, 0x3C, 0xFFFFFFFFu);
    REQUIRE(pe_code_section(img.data(), img.size()).data == nullptr);
    put32(img, 0x3C, 0xFFFFFFE6u);  // +26 == 0 exactly in 32-bit
    REQUIRE(pe_code_section(img.data(), img.size()).data == nullptr);
}

TEST_CASE("pe_code_section rejects a wrapping section raw pointer") {
    // raw_ptr + raw_sz overflowing 32 bits must not pass the bounds check.
    auto img = make_le({ 0xC3 });
    const uint32_t sec = 0x40 + 24;
    put32(img, sec + 16, 0x10);         // SizeOfRawData
    put32(img, sec + 20, 0xFFFFFFF8u);  // PointerToRawData
    REQUIRE(pe_code_section(img.data(), img.size()).data == nullptr);
}

TEST_CASE("pe_code_section survives a section table pushed past the buffer") {
    auto img = make_le({ 0xC3 });
    put16(img, 0x40 + 20, 0xFFFF);  // SizeOfOptionalHeader
    REQUIRE(pe_code_section(img.data(), img.size()).data == nullptr);
}

TEST_CASE("pe_code_section skips zero-size sections") {
    auto img = make_le({ 0xC3 });
    put32(img, 0x40 + 24 + 16, 0);  // SizeOfRawData = 0
    REQUIRE(pe_code_section(img.data(), img.size()).data == nullptr);
}

// ---------------------------------------------------------------------------
// pe_va_to_offset
// ---------------------------------------------------------------------------

TEST_CASE("pe_va_to_offset translates in-section VAs and rejects the rest") {
    std::vector<uint8_t> payload = { 1, 2, 3, 4 };
    auto img = make_le(payload);
    auto cs = pe_code_section(img.data(), img.size());
    REQUIRE(cs.data != nullptr);

    REQUIRE(pe_va_to_offset(cs, 0x1000) == 0);
    REQUIRE(pe_va_to_offset(cs, 0x1003) == 3);
    REQUIRE(pe_va_to_offset(cs, 0x1004) == (size_t)-1);   // one past the end
    REQUIRE(pe_va_to_offset(cs, 0x0FFF) == (size_t)-1);   // below the base
    REQUIRE(pe_va_to_offset(cs, 0xFFFFFFFFu) == (size_t)-1);
}
