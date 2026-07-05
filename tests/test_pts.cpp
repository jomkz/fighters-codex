#include <catch2/catch_test_macros.hpp>
#include <fx/pts.h>
#include <cstring>
#include <vector>

using namespace fx;

// Minimal MZ + "PL" image with one CODE section (the PTS/CAM/MNU container
// family; layout mirrors tests/test_pe.cpp).
static void put16(std::vector<uint8_t>& b, size_t off, uint16_t v) {
    b[off] = (uint8_t)v; b[off + 1] = (uint8_t)(v >> 8);
}
static void put32(std::vector<uint8_t>& b, size_t off, uint32_t v) {
    b[off] = (uint8_t)v;             b[off + 1] = (uint8_t)(v >> 8);
    b[off + 2] = (uint8_t)(v >> 16); b[off + 3] = (uint8_t)(v >> 24);
}

static std::vector<uint8_t> make_pts(const std::vector<uint8_t>& code) {
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

TEST_CASE("pts_info finds the single icon PIC reference") {
    std::vector<uint8_t> code;
    for (const char* s : { "some junk", "ICONF22.PIC", "more" }) {
        code.insert(code.end(), s, s + strlen(s));
        code.push_back(0);
    }
    auto pts = make_pts(code);
    auto info = pts_info(pts.data(), pts.size());
    REQUIRE(info.valid);
    REQUIRE(info.code.vma == 0x1000);
    REQUIRE(info.icon == "ICONF22.PIC");
}

TEST_CASE("pts_info reports a missing icon as empty") {
    std::vector<uint8_t> code = { 'n', 'o', 'p', 'e', 0 };
    auto pts = make_pts(code);
    auto info = pts_info(pts.data(), pts.size());
    REQUIRE(info.valid);
    REQUIRE(info.icon.empty());
}

TEST_CASE("pts_info rejects a non-DLL payload") {
    std::vector<uint8_t> junk = { 'I', 'C', 'O', 'N', '.', 'P', 'I', 'C' };
    REQUIRE_FALSE(pts_info(junk.data(), junk.size()).valid);
}
