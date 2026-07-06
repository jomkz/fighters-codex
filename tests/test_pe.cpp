#include <catch2/catch_test_macros.hpp>
#include <fx/pe.h>
#include <cstring>
#include <vector>

#include "support/le_image.h"

using namespace fx;
using fx_test::make_le;
using fx_test::put16;
using fx_test::put32;

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
