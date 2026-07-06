#include <catch2/catch_test_macros.hpp>
#include <fx/mus.h>
#include <cstdint>
#include <vector>

#include "support/le_image.h"

using namespace fx;
using fx_test::make_le;

namespace {
void u32le(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(x & 0xFF); v.push_back((x >> 8) & 0xFF);
    v.push_back((x >> 16) & 0xFF); v.push_back((x >> 24) & 0xFF);
}
void u24le(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(x & 0xFF); v.push_back((x >> 8) & 0xFF); v.push_back((x >> 16) & 0xFF);
}
const std::vector<uint8_t> kDispatch = {0x01, 0x02, 0x03, 0x02, 0x01, 0x02, 0x03, 0x02, 0x01};
void append(std::vector<uint8_t>& v, const std::vector<uint8_t>& more) {
    v.insert(v.end(), more.begin(), more.end());
}
}  // namespace

TEST_CASE("mus_xmi_name maps index 1 to VALK01 and the rest to AIRnnn") {
    REQUIRE(mus_xmi_name(1) == "VALK01.XMI");
    REQUIRE(mus_xmi_name(3) == "AIR003.XMI");
    REQUIRE(mus_xmi_name(4) == "AIR004.XMI");
    REQUIRE(mus_xmi_name(127) == "AIR127.XMI");
}

TEST_CASE("mus_disassemble returns invalid without a CODE section") {
    std::vector<uint8_t> junk(64, 'X');
    MusScript s = mus_disassemble(junk.data(), junk.size());
    REQUIRE_FALSE(s.valid);
    REQUIRE(s.ops.empty());
}

TEST_CASE("mus_disassemble decodes every opcode form") {
    std::vector<uint8_t> code;
    code.push_back(0xFF); code.push_back('a'); code.push_back('i');
    code.push_back('r'); code.push_back(0x00);              // FF playlist "air"
    code.push_back(0xFA); code.push_back(0x21); u32le(code, 0xDEADBEEF); // FA
    code.push_back(0xFB); code.push_back(0x50); code.push_back(0x03); code.push_back(0xF9); // FB + F9
    code.push_back(0xFB); code.push_back(0x5A); code.push_back(0x01);    // FB short (idx 1)
    code.push_back(0xFC); append(code, kDispatch);          // FC + dispatch
    code.push_back(0xFE); u32le(code, 0x00C0FFEE); append(code, kDispatch); // FE + dispatch
    code.push_back(0xFD); u24le(code, 0x123456);            // FD
    code.push_back(0xF9);                                    // section-end skip
    code.push_back(0x00);                                    // tail padding stop

    auto img = make_le(code);
    MusScript s = mus_disassemble(img.data(), img.size());
    REQUIRE(s.valid);
    REQUIRE_FALSE(s.stopped_early);
    REQUIRE(s.ops.size() == 7u);

    REQUIRE(s.ops[0].op == 0xFF);
    REQUIRE(s.ops[0].playlist_id == "air");

    REQUIRE(s.ops[1].op == 0xFA);
    REQUIRE(s.ops[1].sub == 0x21);
    REQUIRE(s.ops[1].value == 0xDEADBEEFu);

    REQUIRE(s.ops[2].op == 0xFB);
    REQUIRE(s.ops[2].mode == 0x50);
    REQUIRE(s.ops[2].track_idx == 3);
    REQUIRE(s.ops[2].xmi == "AIR003.XMI");

    REQUIRE(s.ops[3].op == 0xFB);           // short form, no F9
    REQUIRE(s.ops[3].track_idx == 1);
    REQUIRE(s.ops[3].xmi == "VALK01.XMI");

    REQUIRE(s.ops[4].op == 0xFC);

    REQUIRE(s.ops[5].op == 0xFE);
    REQUIRE(s.ops[5].value == 0x00C0FFEEu);

    REQUIRE(s.ops[6].op == 0xFD);
    REQUIRE(s.ops[6].value == 0x123456u);

    // Offsets are monotonic within the CODE section.
    for (size_t i = 1; i < s.ops.size(); i++)
        REQUIRE(s.ops[i].offset > s.ops[i - 1].offset);
}

TEST_CASE("mus_disassemble consumes the FC/FE dispatch table only when it matches") {
    // FC followed by non-dispatch bytes: the table is not consumed, so the
    // next byte is decoded as its own opcode.
    std::vector<uint8_t> code = {0xFC, 0xFD, 0x00, 0x00, 0x00};
    auto img = make_le(code);
    MusScript s = mus_disassemble(img.data(), img.size());
    REQUIRE(s.valid);
    REQUIRE(s.ops.size() == 2u);
    REQUIRE(s.ops[0].op == 0xFC);
    REQUIRE(s.ops[1].op == 0xFD);
    REQUIRE(s.ops[1].value == 0u);
}

TEST_CASE("mus_disassemble stops and records an unrecognised byte") {
    std::vector<uint8_t> code = {0xFC, 0x42};  // 0x42 is not an opcode
    auto img = make_le(code);
    MusScript s = mus_disassemble(img.data(), img.size());
    REQUIRE(s.valid);
    REQUIRE(s.stopped_early);
    REQUIRE(s.stop_byte == 0x42);
    REQUIRE(s.ops.size() == 1u);  // the FC before it
}
