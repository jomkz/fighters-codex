// FNT glyph compiler — canonical x86 emission + byte-identical repack (#97).
#include <catch2/catch_test_macros.hpp>
#include <fx/ealib.h>
#include <fx/fnt.h>
#include <fx/pe.h>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace fx;

TEST_CASE("fnt_emit_glyph uses the canonical run encoding") {
    // Empty glyph: a lone RET, no row advances.
    std::vector<uint8_t> blank(4 * 2, 0);
    CHECK(fnt_emit_glyph(blank.data(), 4, 2) == std::vector<uint8_t>{0xC3});

    // One pixel at column 0, row 0: short byte form, then per-row advances.
    std::vector<uint8_t> one(4 * 2, 0);
    one[0] = 0xFF;
    CHECK(fnt_emit_glyph(one.data(), 4, 2) ==
          std::vector<uint8_t>{0x88, 0x07, 0x03, 0xF9, 0x03, 0xF9, 0xC3});

    // A 4-run at column 0 -> dword write; a 2-run at column 1 -> word write
    // with displacement; a 3-run -> word then byte (greedy 4/2/1).
    std::vector<uint8_t> px(8 * 3, 0);
    for (int c = 0; c < 4; ++c) px[c] = 0xFF;              // row 0: 4 @ 0
    for (int c = 1; c < 3; ++c) px[8 + c] = 0xFF;          // row 1: 2 @ 1
    for (int c = 2; c < 5; ++c) px[16 + c] = 0xFF;         // row 2: 3 @ 2
    CHECK(fnt_emit_glyph(px.data(), 8, 3) == std::vector<uint8_t>{
        0x89, 0x07,             // MOV [EDI], EAX
        0x03, 0xF9,
        0x66, 0x89, 0x47, 0x01, // MOV [EDI+1], AX
        0x03, 0xF9,
        0x66, 0x89, 0x47, 0x02, // MOV [EDI+2], AX
        0x88, 0x47, 0x04,       // MOV [EDI+4], AL
        0x03, 0xF9,
        0xC3});
}

TEST_CASE("fnt_repack is byte-identical for every install font") {
    const char* root = std::getenv("FX_FA_ROOT");
    if (!root || !*root)
        SKIP("FX_FA_ROOT not set (real-asset mode runs on the benches)");
    namespace fs = std::filesystem;
    auto lower = [](std::string s) {
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    std::vector<uint8_t> lib;
    for (const auto& de : fs::directory_iterator(root)) {
        if (!de.is_regular_file()) continue;
        if (lower(de.path().filename().string()) != "fa_1.lib") continue;
        std::ifstream f(de.path(), std::ios::binary | std::ios::ate);
        lib.resize((size_t)f.tellg());
        f.seekg(0);
        f.read((char*)lib.data(), (std::streamsize)lib.size());
        break;
    }
    if (lib.empty()) SKIP("FA_1.LIB not present in this install");

    int total = 0;
    for (const auto& e : ealib_read_dir(lib.data(), lib.size())) {
        std::string name = lower(e.name);
        if (name.size() < 4 || name.substr(name.size() - 4) != ".fnt") continue;
        auto fnt_bytes = ealib_extract(lib.data(), lib.size(), e, true);
        INFO(e.name);
        REQUIRE_FALSE(fnt_bytes.empty());

        FntFile fnt = fnt_parse(fnt_bytes.data(), fnt_bytes.size());
        REQUIRE(fnt.valid);
        CodeSection cs = pe_code_section(fnt_bytes.data(), fnt_bytes.size());
        REQUIRE(cs.data != nullptr);
        fnt_render_glyphs(fnt, cs.data, cs.size, cs.vma);
        REQUIRE(fnt.glyphs.size() == 256u);

        auto out = fnt_repack(fnt_bytes.data(), fnt_bytes.size(),
                              fnt.font_height, fnt.glyph_width, fnt.glyphs);
        REQUIRE(out == fnt_bytes);
        ++total;
    }
    REQUIRE(total > 0);
    WARN("FNT repack census: " << total << " fonts byte-identical");
}
