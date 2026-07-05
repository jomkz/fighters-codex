#include <catch2/catch_test_macros.hpp>
#include <fx/dlg.h>
#include <algorithm>
#include <cstring>
#include <vector>

using namespace fx;

// Minimal MZ + "PL" image with one CODE section (the DLG/CAM container
// family; layout mirrors tests/test_pe.cpp).
static void put16(std::vector<uint8_t>& b, size_t off, uint16_t v) {
    b[off] = (uint8_t)v; b[off + 1] = (uint8_t)(v >> 8);
}
static void put32(std::vector<uint8_t>& b, size_t off, uint32_t v) {
    b[off] = (uint8_t)v;             b[off + 1] = (uint8_t)(v >> 8);
    b[off + 2] = (uint8_t)(v >> 16); b[off + 3] = (uint8_t)(v >> 24);
}

static std::vector<uint8_t> make_dlg(const std::vector<uint8_t>& code) {
    const uint32_t pe_off = 0x40;
    const uint32_t raw_ptr = pe_off + 24 + 40;
    std::vector<uint8_t> b(raw_ptr, 0);
    b[0] = 'M'; b[1] = 'Z';
    put32(b, 0x3C, pe_off);
    b[pe_off] = 'P'; b[pe_off + 1] = 'L';
    put16(b, pe_off + 6, 1);
    put16(b, pe_off + 20, 0);
    const uint32_t sec = pe_off + 24;
    memcpy(&b[sec], "CODE", 4);
    put32(b, sec + 8, (uint32_t)code.size());
    put32(b, sec + 12, 0x1000);
    put32(b, sec + 16, (uint32_t)code.size());
    put32(b, sec + 20, raw_ptr);
    b.insert(b.end(), code.begin(), code.end());
    return b;
}

TEST_CASE("dlg_info validates the container and finds the dispatch table") {
    std::vector<uint8_t> code = { 0x10, 0x12, 0x00, 0x00 };  // header thunk VA
    auto dlg = make_dlg(code);
    auto info = dlg_info(dlg.data(), dlg.size());
    REQUIRE(info.valid);
    REQUIRE(info.code.vma == 0x1000);
    REQUIRE(info.code.size == code.size());
}

TEST_CASE("dlg_strings extracts the embedded control labels") {
    std::vector<uint8_t> code;
    for (const char* s : { "Play Single Mission", "Start New Campaign" }) {
        code.insert(code.end(), s, s + strlen(s));
        code.push_back(0);
    }
    auto dlg = make_dlg(code);
    auto strings = dlg_strings(dlg.data(), dlg.size());
    REQUIRE(std::count(strings.begin(), strings.end(),
                       std::string("Play Single Mission")) == 1);
    REQUIRE(std::count(strings.begin(), strings.end(),
                       std::string("Start New Campaign")) == 1);
}

TEST_CASE("dlg_info rejects a non-DLL payload") {
    std::vector<uint8_t> junk = { 'D', 'L', 'G', 0, 1, 2, 3, 4 };
    REQUIRE_FALSE(dlg_info(junk.data(), junk.size()).valid);
}
