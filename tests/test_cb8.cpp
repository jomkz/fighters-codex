// CB8 codec — engine-traced VQ keyframe model (#95).
#include <catch2/catch_test_macros.hpp>
#include <fx/cb8.h>
#include <fx/ealib.h>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace fx;

namespace {

void w32v(std::vector<uint8_t>& v, uint32_t x) {
    for (int i = 0; i < 4; i++) v.push_back((uint8_t)(x >> (8 * i)));
}
void w16v(std::vector<uint8_t>& v, uint16_t x) {
    v.push_back((uint8_t)x);
    v.push_back((uint8_t)(x >> 8));
}

// A minimal valid CB8: DRBC header, VooM index, one 8x8 all-single MRFI.
std::vector<uint8_t> tiny_cb8() {
    std::vector<uint8_t> f;
    f.insert(f.end(), {'D', 'R', 'B', 'C'});
    w32v(f, 0);                   // flags: no audio, not doubled
    w16v(f, 150);                 // audio divisor (observed value; unused here)
    w16v(f, 22050);               // audio rate term (observed value)
    w32v(f, 0x65);                // version (engine requires < 0x67)
    f.resize(64, 0xFF);           // 0xFF header padding

    const uint32_t voom_sz = 20 + 16;
    const uint32_t mrfi_off = 64 + voom_sz;
    f.insert(f.end(), {'V', 'o', 'o', 'M'});
    w32v(f, voom_sz);
    w32v(f, 8);                   // width
    w32v(f, 8);                   // height
    w32v(f, 6000);                // sync ticks per second
    w32v(f, mrfi_off);            // frame 0: offset
    w32v(f, 808);                 //          chunk size (24+768+8+4+4)
    w32v(f, 0);                   //          cumulative audio
    w32v(f, 400);                 //          sync ticks per frame

    f.insert(f.end(), {'M', 'R', 'F', 'I'});
    w32v(f, 808);
    f.push_back(0);               // kind: key
    f.push_back(5);               // submode: 8-bit paletted
    w16v(f, 0);                   // A
    w16v(f, 0);                   // B
    w16v(f, 2);                   // C
    w16v(f, 4);                   // S (4 cells -> one u32 word)
    w16v(f, 768);                 // D
    w16v(f, 0xFFFF);              // X: no band switch
    w16v(f, 0);                   // pad
    std::vector<uint8_t> pal(768, 0);
    pal[1 * 3 + 0] = 63;          // index 1: red
    pal[2 * 3 + 1] = 63;          // index 2: green
    f.insert(f.end(), pal.begin(), pal.end());
    f.insert(f.end(), {1, 1, 2, 2});  // single entry 0
    f.insert(f.end(), {2, 2, 2, 2});  // single entry 1
    f.insert(f.end(), {0, 0, 0, 0});  // mode bitmap: all single
    f.insert(f.end(), {0, 1, 1, 0});  // cell indices TL,TR,BL,BR
    return f;
}

Cb8Frame frame_of(Cb8Decoder* dec, uint32_t i) {
    Cb8Frame fr;
    fr.indices = cb8_decode_frame(dec, i);
    Palette p;
    REQUIRE(cb8_frame_palette(dec, i, &p));
    for (int k = 0; k < 256; k++) {  // un-widen back to the stored 6-bit values
        fr.palette6[(size_t)k * 3 + 0] = (uint8_t)(p.r[k] >> 2);
        fr.palette6[(size_t)k * 3 + 1] = (uint8_t)(p.g[k] >> 2);
        fr.palette6[(size_t)k * 3 + 2] = (uint8_t)(p.b[k] >> 2);
    }
    return fr;
}

}  // namespace

TEST_CASE("cb8 decodes a hand-built keyframe movie") {
    auto file = tiny_cb8();
    Cb8Info info;
    REQUIRE(cb8_info(file.data(), file.size(), &info));
    REQUIRE(info.width == 8);
    REQUIRE(info.height == 8);
    REQUIRE(info.frame_count == 1);

    Cb8Decoder* dec = cb8_open(file.data(), file.size());
    REQUIRE(dec);
    auto px = cb8_decode_frame(dec, 0);
    REQUIRE(px.size() == 64u);
    // Cell 0 = single entry 0 (1,1,2,2), each value doubled to a 2x2 quadrant.
    CHECK(px[0] == 1);
    CHECK(px[3] == 1);         // top-right quadrant of cell 0 is value 1
    CHECK(px[2 * 8 + 0] == 2); // bottom quadrants are value 2
    CHECK(px[0 * 8 + 4] == 2); // cell 1 = entry 1, uniformly 2

    Palette pal;
    REQUIRE(cb8_frame_palette(dec, 0, &pal));
    CHECK(pal.r[1] == 252);    // 6-bit 63 widened per pal_load: (v<<2)|(v>>6)
    CHECK(pal.g[2] == 252);

    auto rgba = cb8_decode_frame_rgba(dec, 0);
    REQUIRE(rgba.size() == 64u * 4u);
    CHECK(rgba[0] == 252);     // pixel 0 wears palette entry 1 = red
    CHECK(rgba[1] == 0);
    cb8_close(dec);
}

TEST_CASE("cb8_repack round-trips edited frames pixel-identically") {
    auto file = tiny_cb8();
    Cb8Decoder* dec = cb8_open(file.data(), file.size());
    REQUIRE(dec);
    Cb8Frame fr = frame_of(dec, 0);
    fr.indices[1] = 7;   // break cell 0's uniformity -> a detail cell
    fr.indices[60] = 9;  // and touch the last cell
    cb8_close(dec);

    auto out = cb8_repack(file.data(), file.size(), {fr});
    REQUIRE_FALSE(out.empty());
    Cb8Decoder* dec2 = cb8_open(out.data(), out.size());
    REQUIRE(dec2);
    auto px = cb8_decode_frame(dec2, 0);
    REQUIRE(px == fr.indices);
    cb8_close(dec2);
}

TEST_CASE("cb8 full-movie repack is pixel-identical on a real install movie") {
    const char* root = std::getenv("FX_FA_ROOT");
    if (!root || !*root)
        SKIP("FX_FA_ROOT not set (real-asset mode runs on the benches)");
    namespace fs = std::filesystem;
    auto lower = [](std::string s) {
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    // JANELOGO.CB8 ships in fa_4c.lib (case varies across installs).
    std::vector<uint8_t> lib;
    for (const auto& de : fs::directory_iterator(root)) {
        if (!de.is_regular_file()) continue;
        if (lower(de.path().filename().string()) != "fa_4c.lib") continue;
        std::ifstream f(de.path(), std::ios::binary | std::ios::ate);
        lib.resize((size_t)f.tellg());
        f.seekg(0);
        f.read((char*)lib.data(), (std::streamsize)lib.size());
        break;
    }
    if (lib.empty()) SKIP("fa_4c.lib not present in this install");

    auto entries = ealib_read_dir(lib.data(), lib.size());
    const Entry* e = ealib_find(entries, "JANELOGO.CB8");
    REQUIRE(e != nullptr);
    auto movie = ealib_extract(lib.data(), lib.size(), *e, true);
    REQUIRE_FALSE(movie.empty());

    Cb8Info info;
    REQUIRE(cb8_info(movie.data(), movie.size(), &info));
    Cb8Decoder* dec = cb8_open(movie.data(), movie.size());
    REQUIRE(dec);
    std::vector<Cb8Frame> frames;
    frames.reserve(info.frame_count);
    for (uint32_t i = 0; i < info.frame_count; i++) {
        frames.push_back(frame_of(dec, i));
        REQUIRE_FALSE(frames.back().indices.empty());
    }

    auto out = cb8_repack(movie.data(), movie.size(), frames);
    REQUIRE_FALSE(out.empty());
    Cb8Decoder* dec2 = cb8_open(out.data(), out.size());
    REQUIRE(dec2);
    for (uint32_t i = 0; i < info.frame_count; i++) {
        INFO("frame " << i);
        REQUIRE(cb8_decode_frame(dec2, i) == frames[i].indices);
    }
    cb8_close(dec2);
    cb8_close(dec);
}
