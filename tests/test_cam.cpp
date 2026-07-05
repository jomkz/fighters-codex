#include <catch2/catch_test_macros.hpp>
#include <fx/cam.h>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

using namespace fx;

// ---------------------------------------------------------------------------
// Helpers — minimal MZ + "PL" image with one CODE section (the CAM container
// shape; see CAM.md and pe.h). Layout mirrors tests/test_pe.cpp.
// ---------------------------------------------------------------------------

static void put16(std::vector<uint8_t>& b, size_t off, uint16_t v) {
    b[off] = (uint8_t)v; b[off + 1] = (uint8_t)(v >> 8);
}
static void put32(std::vector<uint8_t>& b, size_t off, uint32_t v) {
    b[off] = (uint8_t)v;             b[off + 1] = (uint8_t)(v >> 8);
    b[off + 2] = (uint8_t)(v >> 16); b[off + 3] = (uint8_t)(v >> 24);
}

static std::vector<uint8_t> make_cam(const std::vector<uint8_t>& code) {
    const uint32_t pe_off = 0x40;
    const uint32_t raw_ptr = pe_off + 24 + 40;
    std::vector<uint8_t> b(raw_ptr, 0);
    b[0] = 'M'; b[1] = 'Z';
    put32(b, 0x3C, pe_off);
    b[pe_off] = 'P'; b[pe_off + 1] = 'L';
    put16(b, pe_off + 6, 1);   // NumberOfSections
    put16(b, pe_off + 20, 0);  // SizeOfOptionalHeader
    const uint32_t sec = pe_off + 24;
    memcpy(&b[sec], "CODE", 4);
    put32(b, sec + 8, (uint32_t)code.size());
    put32(b, sec + 12, 0x1000);
    put32(b, sec + 16, (uint32_t)code.size());
    put32(b, sec + 20, raw_ptr);
    b.insert(b.end(), code.begin(), code.end());
    return b;
}

// ---------------------------------------------------------------------------

TEST_CASE("cam_info validates the container and finds the CODE section") {
    std::vector<uint8_t> code = { 0xC3, 0x90, 0x90 };
    auto cam = make_cam(code);
    auto info = cam_info(cam.data(), cam.size());
    REQUIRE(info.valid);
    REQUIRE(info.code.size == code.size());
    REQUIRE(info.code.vma == 0x1000);
}

TEST_CASE("cam_info rejects a non-DLL payload") {
    std::vector<uint8_t> junk = { 't', 'e', 'x', 't', 'F', 'o', 'r', 'm' };
    REQUIRE_FALSE(cam_info(junk.data(), junk.size()).valid);
}

TEST_CASE("cam_strings extracts the embedded string tables") {
    // The campaign config shape (CAM.md): null-terminated mission ids,
    // filenames, and state keys, separated by non-printable bytes.
    std::vector<uint8_t> code;
    for (const char* s : { "U01I", "~U01.M", "UMEDAL", "FA_4C.LIB" }) {
        code.insert(code.end(), s, s + strlen(s));
        code.push_back(0);
    }
    code.push_back(1); code.push_back('a'); code.push_back('b'); // len-2 run
    auto cam = make_cam(code);

    auto strings = cam_strings(cam.data(), cam.size());
    // The section table's "CODE" name also extracts — parsing the whole
    // file, as the GUI and `fx cam strings` do.
    REQUIRE(std::count(strings.begin(), strings.end(), "U01I") == 1);
    REQUIRE(std::count(strings.begin(), strings.end(), "~U01.M") == 1);
    REQUIRE(std::count(strings.begin(), strings.end(), "UMEDAL") == 1);
    REQUIRE(std::count(strings.begin(), strings.end(), "FA_4C.LIB") == 1);
    // Runs shorter than min_len are discarded.
    REQUIRE(std::count(strings.begin(), strings.end(), "ab") == 0);
}

TEST_CASE("cam_strings honors min_len") {
    std::vector<uint8_t> data = { 'a', 'b', 0, 'c', 'd', 'e', 'f', 0 };
    REQUIRE(cam_strings(data.data(), data.size(), 4).size() == 1);
    REQUIRE(cam_strings(data.data(), data.size(), 2).size() == 2);
}
