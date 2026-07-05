#include <catch2/catch_test_macros.hpp>
#include <fx/hgr.h>
#include <cstring>
#include <vector>

using namespace fx;

// Minimal MZ + "PL" image with one CODE section (the HGR/CAM container
// family; layout mirrors tests/test_pe.cpp).
static void put16(std::vector<uint8_t>& b, size_t off, uint16_t v) {
    b[off] = (uint8_t)v; b[off + 1] = (uint8_t)(v >> 8);
}
static void put32(std::vector<uint8_t>& b, size_t off, uint32_t v) {
    b[off] = (uint8_t)v;             b[off + 1] = (uint8_t)(v >> 8);
    b[off + 2] = (uint8_t)(v >> 16); b[off + 3] = (uint8_t)(v >> 24);
}

static std::vector<uint8_t> make_hgr(const std::vector<uint8_t>& code) {
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

TEST_CASE("hgr_info collects the referenced PIC assets in order") {
    std::vector<uint8_t> code;
    for (const char* s : { "h_airb.PIC", "junk", "SELICONS.PIC" }) {
        code.insert(code.end(), s, s + strlen(s));
        code.push_back(0);
    }
    auto hgr = make_hgr(code);
    auto info = hgr_info(hgr.data(), hgr.size());
    REQUIRE(info.valid);
    REQUIRE(info.pics ==
            std::vector<std::string>{ "h_airb.PIC", "SELICONS.PIC" });
}

TEST_CASE("hgr_info reports no PICs as an empty list") {
    std::vector<uint8_t> code = { 'x', 0 };
    auto hgr = make_hgr(code);
    auto info = hgr_info(hgr.data(), hgr.size());
    REQUIRE(info.valid);
    REQUIRE(info.pics.empty());
}

TEST_CASE("hgr_info rejects a non-DLL payload") {
    std::vector<uint8_t> junk = { 'H', 'G', 'R', '!', 0, 0, 0, 0 };
    REQUIRE_FALSE(hgr_info(junk.data(), junk.size()).valid);
}
