#include <catch2/catch_test_macros.hpp>
#include <fx/bi.h>
#include <cstring>
#include <string>
#include <vector>

#include "support/le_image.h"

using namespace fx;
using fx_test::make_le;

// The disassembly text lines, in order, for snapshot comparison.
static std::vector<std::string> disasm_text(const std::vector<uint8_t>& img) {
    std::vector<std::string> out;
    for (const auto& in : bi_disasm(img.data(), img.size()))
        out.push_back(in.text);
    return out;
}

static void put_u16(std::vector<uint8_t>& v, size_t at, uint16_t x) {
    v[at] = (uint8_t)(x & 0xFF); v[at + 1] = (uint8_t)(x >> 8);
}
static void put_u32(std::vector<uint8_t>& v, size_t at, uint32_t x) {
    v[at + 0] = (uint8_t)(x);       v[at + 1] = (uint8_t)(x >> 8);
    v[at + 2] = (uint8_t)(x >> 16); v[at + 3] = (uint8_t)(x >> 24);
}

// A minimal MZ/Phar-Lap-LE container with a CODE and a .idata section — enough
// to reach build_import_map. The caller sets .idata's SizeOfRawData so the
// bounds-check overflow path can be exercised.
static std::vector<uint8_t> make_bi_container(uint32_t idata_raw_sz) {
    const uint32_t PE = 0x40, SEC = PE + 24, S1 = SEC + 40, BODY = S1 + 40;
    std::vector<uint8_t> buf(BODY + 64, 0);
    buf[0] = 'M'; buf[1] = 'Z';
    put_u32(buf, 0x3C, PE);
    buf[PE + 0] = 'P'; buf[PE + 1] = 'L';   // "PL\0\0"
    put_u16(buf, PE + 6, 2);                 // NumSections = 2
    put_u16(buf, PE + 20, 0);                // SizeOfOptionalHeader = 0
    std::memcpy(buf.data() + SEC, "CODE", 4);
    put_u32(buf, SEC + 16, 16);              // CODE SizeOfRawData
    put_u32(buf, SEC + 20, BODY);            // CODE PointerToRawData
    std::memcpy(buf.data() + S1, ".idata", 6);
    put_u32(buf, S1 + 16, idata_raw_sz);     // .idata SizeOfRawData (test value)
    put_u32(buf, S1 + 20, BODY + 16);        // .idata PointerToRawData
    return buf;
}

TEST_CASE("bi_disasm on empty input returns nothing") {
    REQUIRE(bi_disasm(nullptr, 0).empty());
}

// Regression (#118 fuzz): a huge PE-header offset must be rejected, not wrapped
// by a 32-bit bounds check into an out-of-bounds header read.
TEST_CASE("bi_disasm rejects an out-of-range PE header offset") {
    std::vector<uint8_t> buf(0x80, 0);
    buf[0] = 'M'; buf[1] = 'Z';
    put_u32(buf, 0x3C, 0xFFFFFFF0u);
    REQUIRE(bi_disasm(buf.data(), buf.size()).empty());
}

// Regression (#118 fuzz): an .idata section whose SizeOfRawData is near
// UINT32_MAX must not pass raw_ptr+raw_sz <= size by wrapping — that walked the
// import table far past the buffer (out-of-bounds read).
TEST_CASE("bi_disasm survives a .idata section with an overflowing raw size") {
    auto buf = make_bi_container(0xFFFFFFFFu);
    auto instrs = bi_disasm(buf.data(), buf.size());  // must not read OOB / crash
    (void)instrs;
    SUCCEED();
}

// A well-formed but import-less container disassembles without error.
TEST_CASE("bi_disasm handles a container with a bounded empty .idata") {
    auto buf = make_bi_container(20);
    auto instrs = bi_disasm(buf.data(), buf.size());
    (void)instrs;
    SUCCEED();
}

// Dump snapshot (#114): a hand-crafted opcode stream disassembles to a fixed
// listing — push/load/store, an arithmetic op, EVAL, END.
TEST_CASE("bi_disasm renders a stable listing for a value expression") {
    auto img = make_le({
        0x03, 0x05,   // PUSH_BYTE 5
        0x06, 0x01,   // LOAD_VAR %b
        0x0B,         // ADD
        0x05, 0x00,   // STORE_VAR %a
        0x04,         // EVAL
        0x25,         // END
    });
    REQUIRE(disasm_text(img) == std::vector<std::string>{
        "PUSH_BYTE 5", "LOAD_VAR %b", "ADD", "STORE_VAR %a", "EVAL", "END",
    });
}

// Dump snapshot (#114): a forward GOTO plants a jump target, and the target
// instruction is annotated with an @offset label in the listing.
TEST_CASE("bi_disasm annotates a jump target with a label") {
    auto img = make_le({
        0x20, 0x05, 0x00,   // GOTO @0005  (offset value 5)
        0x04,               // EVAL
        0x04,               // EVAL
        0x03, 0x07,         // @0005: PUSH_BYTE 7
        0x25,               // END
    });
    REQUIRE(disasm_text(img) == std::vector<std::string>{
        "GOTO @0005", "EVAL", "EVAL", "@0005:", "PUSH_BYTE 7", "END",
    });
}
