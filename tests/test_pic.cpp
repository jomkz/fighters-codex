#include <catch2/catch_test_macros.hpp>
#include <fx/pic.h>
#include <fx/pal.h>
#include <fx/ealib.h>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace fx;

static std::vector<uint8_t> make_rgba(int w, int h, uint8_t r, uint8_t g, uint8_t b) {
    std::vector<uint8_t> buf((size_t)w * h * 4);
    for (int i = 0; i < w * h; i++) {
        buf[i*4+0] = r;
        buf[i*4+1] = g;
        buf[i*4+2] = b;
        buf[i*4+3] = 255;
    }
    return buf;
}

TEST_CASE("pic_info rejects empty buffer") {
    uint8_t tiny[1] = {0};
    PicInfo info;
    REQUIRE_FALSE(pic_info(tiny, 0, &info));
}

TEST_CASE("pic_info rejects truncated buffer") {
    uint8_t buf[2] = {0, 0};
    PicInfo info;
    REQUIRE_FALSE(pic_info(buf, 2, &info));
}

TEST_CASE("pic_encode returns non-empty output") {
    auto rgba = make_rgba(8, 8, 200, 100, 50);
    Palette pal = {};
    auto encoded = pic_encode(rgba.data(), 8, 8, pal);
    REQUIRE_FALSE(encoded.empty());
}

TEST_CASE("pic_encode produces a parseable dense header with correct dimensions") {
    auto rgba = make_rgba(16, 16, 200, 100, 50);
    Palette pal = {};
    auto encoded = pic_encode(rgba.data(), 16, 16, pal);
    REQUIRE_FALSE(encoded.empty());

    PicInfo info;
    REQUIRE(pic_info(encoded.data(), encoded.size(), &info));
    REQUIRE(info.width  == 16u);
    REQUIRE(info.height == 16u);
    REQUIRE(info.format == 0u); // dense format
}

TEST_CASE("pic_encode / pic_decode round-trip produces correct buffer size") {
    auto rgba = make_rgba(8, 8, 128, 64, 255);
    Palette pal = {};
    auto encoded = pic_encode(rgba.data(), 8, 8, pal);
    REQUIRE_FALSE(encoded.empty());

    auto decoded = pic_decode(encoded.data(), encoded.size(), nullptr);
    REQUIRE(decoded.size() == 8u * 8u * 4u);
}

TEST_CASE("pic_decode on encoded solid-color image returns opaque pixels") {
    auto rgba = make_rgba(4, 4, 0, 0, 255); // solid blue
    Palette pal = {};
    auto encoded = pic_encode(rgba.data(), 4, 4, pal);
    REQUIRE_FALSE(encoded.empty());

    auto decoded = pic_decode(encoded.data(), encoded.size(), nullptr);
    REQUIRE(decoded.size() == 4u * 4u * 4u);
    // All pixels should be fully opaque (alpha = 255)
    for (int i = 3; i < (int)decoded.size(); i += 4)
        REQUIRE(decoded[i] == 255);
}

TEST_CASE("pic_decode on empty data returns empty vector") {
    uint8_t tiny[1] = {0};
    auto decoded = pic_decode(tiny, 0, nullptr);
    REQUIRE(decoded.empty());
}

TEST_CASE("pic_repack round-trips a dense PIC byte-identically") {
    auto rgba = make_rgba(16, 8, 10, 20, 30);
    Palette pal = {};
    auto pic = pic_encode(rgba.data(), 16, 8, pal);
    REQUIRE_FALSE(pic.empty());
    auto out = pic_repack(pic.data(), pic.size());
    REQUIRE(out == pic);
}

TEST_CASE("pic_repack round-trips a synthetic sparse PIC byte-identically") {
    // Layout: header 64 | palette 6 @64 | spans 30 @70 (2 records +
    // terminator) | pixel runs 8 @100 -> 108 bytes, fully covered.
    std::vector<uint8_t> pic(108, 0);
    auto w16 = [&](int o, uint16_t v) { pic[(size_t)o] = (uint8_t)v; pic[(size_t)o + 1] = (uint8_t)(v >> 8); };
    auto w32v = [&](int o, uint32_t v) {
        pic[(size_t)o] = (uint8_t)v;
        pic[(size_t)o + 1] = (uint8_t)(v >> 8);
        pic[(size_t)o + 2] = (uint8_t)(v >> 16);
        pic[(size_t)o + 3] = (uint8_t)(v >> 24);
    };
    w16(0, 1);              // sparse
    w32v(2, 8);  w32v(6, 2);    // 8 x 2
    w32v(10, 100); w32v(14, 8); // pixel runs
    w32v(18, 64);  w32v(22, 6); // palette (2 colours)
    w32v(26, 70);  w32v(30, 30);// spans
    w32v(34, 0);   w32v(38, 0); // no rowheads
    pic[50] = 0xAB;             // header-tail sentinel: must carry verbatim
    w16(70, 0); w16(72, 1); w16(74, 4); w32v(76, 100);  // row 0, x 1..4
    w16(80, 1); w16(82, 0); w16(84, 3); w32v(86, 104);  // row 1, x 0..3
    w16(90, 0xFFFF);                                    // terminator record
    for (int i = 0; i < 8; i++) pic[(size_t)(100 + i)] = (uint8_t)(i + 1);

    auto out = pic_repack(pic.data(), pic.size());
    REQUIRE(out == pic);
    auto rgba = pic_decode(pic.data(), pic.size(), nullptr);  // decode sanity
    REQUIRE(rgba.size() == 8u * 2u * 4u);
}

TEST_CASE("pic_repack passes JPEG PICs through whole-file") {
    // 0xFFD8 SOI reads as format 0xD8FF; the file IS the JPEG stream.
    std::vector<uint8_t> j = {0xFF, 0xD8, 0xFF, 0xE0, 1, 2, 3, 4, 5};
    auto out = pic_repack(j.data(), j.size());
    REQUIRE(out == j);
}

TEST_CASE("pic_repack accepts 16-alignment zero padding before a region") {
    // The install layout: sparse pixel runs end mid-paragraph and the span
    // table starts at the next 16-byte boundary over zero padding.
    std::vector<uint8_t> pic(120, 0);
    auto w16 = [&](int o, uint16_t v) { pic[(size_t)o] = (uint8_t)v; pic[(size_t)o + 1] = (uint8_t)(v >> 8); };
    auto w32v = [&](int o, uint32_t v) {
        for (int k = 0; k < 4; k++) pic[(size_t)(o + k)] = (uint8_t)(v >> (8 * k));
    };
    w16(0, 1);
    w32v(2, 8); w32v(6, 2);
    w32v(10, 64); w32v(14, 12);  // runs 64..76
    w32v(18, 0);  w32v(22, 0);
    w32v(26, 80); w32v(30, 40);  // spans at the 16-boundary 80, 4 records
    w16(80, 0); w16(82, 0); w16(84, 3); w32v(86, 64);
    w16(90, 1); w16(92, 0); w16(94, 3); w32v(96, 68);
    w16(100, 0); w16(102, 4); w16(104, 7); w32v(106, 72);
    w16(110, 0xFFFF);            // terminator
    for (int i = 0; i < 12; i++) pic[(size_t)(64 + i)] = (uint8_t)(i + 1);

    auto out = pic_repack(pic.data(), pic.size());
    REQUIRE(out == pic);

    pic[77] = 1;  // a nonzero pad byte is unknown structure
    REQUIRE(pic_repack(pic.data(), pic.size()).empty());
}

TEST_CASE("pic_repack carries the trailing font block named by header 0x2A") {
    auto rgba = make_rgba(8, 8, 5, 6, 7);
    Palette pal = {};
    auto pic = pic_encode(rgba.data(), 8, 8, pal);
    REQUIRE_FALSE(pic.empty());
    REQUIRE(pic.size() % 16 == 0);  // this layout lands 16-aligned
    const uint32_t font_off = (uint32_t)pic.size();
    pic.resize(pic.size() + 1536, 0xEE);  // 256 x 6-byte glyph records
    for (int k = 0; k < 4; k++) pic[(size_t)(42 + k)] = (uint8_t)(font_off >> (8 * k));

    auto out = pic_repack(pic.data(), pic.size());
    REQUIRE(out == pic);
}

TEST_CASE("pic_repack fails on bytes no region accounts for") {
    auto rgba = make_rgba(8, 8, 1, 2, 3);
    Palette pal = {};
    auto pic = pic_encode(rgba.data(), 8, 8, pal);
    REQUIRE_FALSE(pic.empty());
    pic.push_back(0x00);  // trailing byte the header does not describe
    REQUIRE(pic_repack(pic.data(), pic.size()).empty());
}

TEST_CASE("pic_repack is byte-identical for every PIC in the FA install") {
    const char* root = std::getenv("FX_FA_ROOT");
    if (!root || !*root)
        SKIP("FX_FA_ROOT not set (real-asset mode runs on the benches)");
    namespace fs = std::filesystem;

    auto lower = [](std::string s) {
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };

    int total = 0, dense = 0, sparse = 0, jpeg = 0;
    for (const auto& de : fs::directory_iterator(root)) {
        if (!de.is_regular_file()) continue;
        if (lower(de.path().extension().string()) != ".lib") continue;

        std::ifstream f(de.path(), std::ios::binary | std::ios::ate);
        std::vector<uint8_t> lib((size_t)f.tellg());
        f.seekg(0);
        f.read((char*)lib.data(), (std::streamsize)lib.size());

        for (const auto& e : ealib_read_dir(lib.data(), lib.size())) {
            std::string name = lower(e.name);
            if (name.size() < 4 || name.substr(name.size() - 4) != ".pic") continue;
            auto pic = ealib_extract(lib.data(), lib.size(), e, true);
            INFO(de.path().filename().string() << " : " << e.name);
            REQUIRE_FALSE(pic.empty());
            auto out = pic_repack(pic.data(), pic.size());
            REQUIRE(out == pic);
            ++total;
            const uint16_t fmt = (uint16_t)(pic[0] | (pic[1] << 8));
            if (fmt == 0) ++dense;
            else if (fmt == 1) ++sparse;
            else if (fmt == 0xD8FF) ++jpeg;
        }
    }
    REQUIRE(total > 0);
    WARN("PIC repack census: total=" << total << " dense=" << dense
         << " sparse=" << sparse << " jpeg=" << jpeg);
}
