#include <catch2/catch_test_macros.hpp>
#include <fx/mus.h>
#include <fx/ealib.h>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
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
    // FD is a count-prefixed track list, not a u24 target (#491). This fixture used to
    // build `FD <u24>` — the codec's own assumption — and so could never have caught the
    // bug it was meant to guard.
    code.push_back(0xFD); code.push_back(0x03);
    code.push_back(0x0F); code.push_back(0x16); code.push_back(0x27);
    // A bare index under the running FB mode (0x5A, from the short-form FB above).
    code.push_back(0x11); code.push_back(0xF9);
    code.push_back(0xF9);                                    // section-end skip
    code.push_back(0x00);                                    // tail padding stop

    auto img = make_le(code);
    MusScript s = mus_disassemble(img.data(), img.size());
    REQUIRE(s.valid);
    REQUIRE_FALSE(s.stopped_early);
    REQUIRE(s.ops.size() == 8u);

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
    REQUIRE(s.ops[6].tracks == std::vector<uint8_t>{0x0F, 0x16, 0x27});

    REQUIRE(s.ops[7].op == 0xFB);            // running status: a bare index, last FB's mode
    REQUIRE(s.ops[7].mode == 0x5A);
    REQUIRE(s.ops[7].track_idx == 0x11);

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

// ---------------------------------------------------------------------------
// Real-asset decode census (#491).
//
// MUS is read-only, so there is no round-trip to hide behind: the only proof a playlist
// was understood is that it disassembles to its end. Three of the nine shipped playlists
// did not. FD was read as a fixed `FD <u24>` (what MUS.md said), but its operand is a
// COUNT-PREFIXED LIST — so the reader landed mid-list and aborted on a track index it
// mistook for a bad opcode. M_AIR additionally ends with bare indices under the last FB
// mode (running status).
//
// The oracle: a playlist ends at its dispatch table and zero padding. Anything else means
// the disassembler lost the thread.
TEST_CASE("every real MUS playlist disassembles to the end") {
    const char* root = std::getenv("FX_FA_ROOT");
    if (!root || !*root)
        SKIP("FX_FA_ROOT not set (real-asset mode runs on the benches)");
    namespace fs = std::filesystem;
    auto upper = [](std::string s) {
        for (char& c : s) c = (char)std::toupper((unsigned char)c);
        return s;
    };

    int total = 0, with_list = 0;
    for (const auto& de : fs::directory_iterator(root)) {
        if (!de.is_regular_file()) continue;
        std::string fn = upper(de.path().filename().string());
        if (fn.size() < 4 || fn.substr(fn.size() - 4) != ".LIB") continue;
        std::ifstream f(de.path(), std::ios::binary | std::ios::ate);
        std::vector<uint8_t> lib((size_t)f.tellg());
        f.seekg(0);
        f.read((char*)lib.data(), (std::streamsize)lib.size());

        for (const auto& e : ealib_read_dir(lib.data(), lib.size())) {
            std::string name = upper(e.name);
            if (name.size() < 4 || name.substr(name.size() - 4) != ".MUS") continue;
            auto bytes = ealib_extract(lib.data(), lib.size(), e, true);
            REQUIRE_FALSE(bytes.empty());

            MusScript s = mus_disassemble(bytes.data(), bytes.size());
            INFO(e.name << " stopped at byte 0x" << std::hex << (int)s.stop_byte);
            REQUIRE(s.valid);
            CHECK_FALSE(s.stopped_early);   // the whole point: no playlist may abort
            CHECK(s.ops.size() > 1u);
            for (const auto& op : s.ops)
                if (op.op == 0xFD) {
                    CHECK_FALSE(op.tracks.empty());   // FD carries a list, never nothing
                    ++with_list;
                }
            ++total;
        }
    }
    REQUIRE(total > 0);
    REQUIRE(with_list > 0);
    WARN("MUS census: " << total << " playlists disassembled clean, "
                        << with_list << " FD track lists");
}
