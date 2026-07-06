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

#include "support/le_image.h"

using namespace fx;

namespace {

// A synthetic FNT in canonical form: FONT struct (height, va[256],
// width[256]) followed by the 256 glyph bodies emitted in character order —
// the same layout fnt_repack produces, so repacking unedited glyphs must
// reproduce the file byte-for-byte.
std::vector<uint8_t> make_fnt(uint32_t height,
                              const std::vector<FntGlyph>& glyphs) {
    REQUIRE(glyphs.size() == 256u);
    const uint32_t vma = 0x1000;
    const size_t kStruct = 4 + 256 * 4 + 256 * 4;
    std::vector<uint8_t> payload(kStruct, 0);
    fx_test::put32(payload, 0, height);

    std::vector<uint8_t> code;
    for (int i = 0; i < 256; ++i) {
        const FntGlyph& g = glyphs[(size_t)i];
        fx_test::put32(payload, 4 + (size_t)i * 4,
                       (uint32_t)(vma + kStruct + code.size()));
        fx_test::put32(payload, 4 + 1024 + (size_t)i * 4, g.width);
        auto body = g.pixels.empty()
            ? std::vector<uint8_t>{0xC3}
            : fnt_emit_glyph(g.pixels.data(), g.width, g.height);
        code.insert(code.end(), body.begin(), body.end());
    }
    payload.insert(payload.end(), code.begin(), code.end());
    return fx_test::make_le(payload);
}

// 256 blank glyphs of uniform width, with one patterned 'A'.
std::vector<FntGlyph> sample_glyphs(uint32_t width, uint32_t height) {
    std::vector<FntGlyph> glyphs(256);
    for (int i = 0; i < 256; ++i) {
        glyphs[(size_t)i].ch = (uint8_t)i;
        glyphs[(size_t)i].width = width;
        glyphs[(size_t)i].height = height;
        glyphs[(size_t)i].pixels.assign((size_t)width * height, 0);
    }
    auto& a = glyphs[(size_t)'A'].pixels;
    for (uint32_t c = 0; c < 5; ++c) a[c] = 0xFF;              // 4+1 run
    a[width + 1] = 0xFF; a[width + 2] = 0xFF;                  // 2-run @ 1
    a[2 * width + 3] = 0xFF;                                   // 1 @ 3
    return glyphs;
}

}  // namespace

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

TEST_CASE("fnt synthetic font round-trips byte-identically") {
    auto img = make_fnt(3, sample_glyphs(8, 3));

    FntFile fnt = fnt_parse(img.data(), img.size());
    REQUIRE(fnt.valid);
    REQUIRE(fnt.font_height == 3u);
    REQUIRE(fnt.glyph_width['A'] == 8u);

    CodeSection cs = pe_code_section(img.data(), img.size());
    REQUIRE(cs.data != nullptr);
    fnt_render_glyphs(fnt, cs.data, cs.size, cs.vma);
    REQUIRE(fnt.glyphs.size() == 256u);
    // The rendered 'A' reproduces the authored pattern exactly.
    REQUIRE(fnt.glyphs[(size_t)'A'].pixels == sample_glyphs(8, 3)[(size_t)'A'].pixels);

    auto out = fnt_repack(img.data(), img.size(),
                          fnt.font_height, fnt.glyph_width, fnt.glyphs);
    REQUIRE(out == img);
}

TEST_CASE("fnt_repack applies a shrinking glyph edit in place") {
    auto img = make_fnt(3, sample_glyphs(8, 3));
    FntFile fnt = fnt_parse(img.data(), img.size());
    REQUIRE(fnt.valid);
    CodeSection cs = pe_code_section(img.data(), img.size());
    REQUIRE(cs.data != nullptr);
    fnt_render_glyphs(fnt, cs.data, cs.size, cs.vma);

    // Clearing pixels shrinks the re-emitted body, which fits the original
    // region (growth is the error path below).
    auto edited = fnt.glyphs;
    std::fill(edited[(size_t)'A'].pixels.begin(),
              edited[(size_t)'A'].pixels.end(), (uint8_t)0);
    auto out = fnt_repack(img.data(), img.size(),
                          fnt.font_height, fnt.glyph_width, edited);
    REQUIRE_FALSE(out.empty());

    FntFile fnt2 = fnt_parse(out.data(), out.size());
    REQUIRE(fnt2.valid);
    CodeSection cs2 = pe_code_section(out.data(), out.size());
    REQUIRE(cs2.data != nullptr);
    fnt_render_glyphs(fnt2, cs2.data, cs2.size, cs2.vma);
    REQUIRE(fnt2.glyphs[(size_t)'A'].pixels ==
            std::vector<uint8_t>((size_t)8 * 3, 0));
}

TEST_CASE("fnt_parse and fnt_repack reject invalid input") {
    // Not a PE image at all.
    std::vector<uint8_t> junk(64, 0xAB);
    CHECK_FALSE(fnt_parse(junk.data(), junk.size()).valid);

    // Valid LE image, but the CODE section is smaller than the FONT struct.
    auto tiny = fx_test::make_le({0xC3});
    CHECK_FALSE(fnt_parse(tiny.data(), tiny.size()).valid);

    auto img = make_fnt(3, sample_glyphs(8, 3));
    FntFile fnt = fnt_parse(img.data(), img.size());
    REQUIRE(fnt.valid);
    CodeSection cs = pe_code_section(img.data(), img.size());
    REQUIRE(cs.data != nullptr);
    fnt_render_glyphs(fnt, cs.data, cs.size, cs.vma);

    // Wrong glyph count.
    auto short_set = fnt.glyphs;
    short_set.pop_back();
    CHECK(fnt_repack(img.data(), img.size(),
                     fnt.font_height, fnt.glyph_width, short_set).empty());

    // Growth past the original body region: lighting up a blank glyph makes
    // the re-emitted code overrun, which must fail rather than corrupt.
    auto grown = fnt.glyphs;
    std::fill(grown[(size_t)'B'].pixels.begin(),
              grown[(size_t)'B'].pixels.end(), (uint8_t)0xFF);
    CHECK(fnt_repack(img.data(), img.size(),
                     fnt.font_height, fnt.glyph_width, grown).empty());
}

// fuzz_fnt (#117): fnt_repack's glyph-body walk read cs.data[pc+1]/[pc+2]
// for its displacement disambiguation without bounds-checking, so a body
// truncated at the CODE section end read past it. The walk now bounds those
// reads and rejects a body that never reaches a RET.
TEST_CASE("fnt_repack rejects a glyph body truncated at the section end") {
    const uint32_t vma = 0x1000;
    const size_t kStruct = 4 + 256 * 4 + 256 * 4;
    std::vector<uint8_t> payload(kStruct + 1, 0);
    fx_test::put32(payload, 0, 1);  // font_height = 1
    for (int i = 0; i < 256; ++i) {
        fx_test::put32(payload, 4 + (size_t)i * 4, (uint32_t)(vma + kStruct));
        fx_test::put32(payload, 4 + 1024 + (size_t)i * 4, 8);
    }
    // A 0x66-prefixed op as the section's final byte: no room for its operand
    // byte, and no RET follows.
    payload[kStruct] = 0x66;
    auto img = fx_test::make_le(payload);

    FntFile fnt = fnt_parse(img.data(), img.size());
    REQUIRE(fnt.valid);
    CodeSection cs = pe_code_section(img.data(), img.size());
    REQUIRE(cs.data != nullptr);
    fnt_render_glyphs(fnt, cs.data, cs.size, cs.vma);
    CHECK(fnt_repack(img.data(), img.size(), fnt.font_height,
                     fnt.glyph_width, fnt.glyphs).empty());
}
