// LAY write path — parse→repack byte-identity and guarded edits (#99).
#include <catch2/catch_test_macros.hpp>
#include <fx/ealib.h>
#include <fx/lay.h>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "support/le_image.h"

using namespace fx;

namespace {

constexpr uint32_t kVma = 0x1000;
constexpr size_t kHdr = 0x78;
constexpr size_t kLayerSz = 0x160;

// A synthetic LAY CODE payload: the 0x78-byte DLL data header followed by a
// two-entry LAYER array (the second carries the end sentinel).
std::vector<uint8_t> lay_payload() {
    std::vector<uint8_t> p(kHdr + 2 * kLayerSz, 0);
    const uint32_t l0 = kVma + (uint32_t)kHdr;
    const uint32_t l1 = l0 + (uint32_t)kLayerSz;
    fx_test::put32(p, 0x14, 100);                       // sky_angle_scale
    for (int i = 0; i < 10; ++i) fx_test::put32(p, 0x18 + (size_t)i * 4, l0);
    fx_test::put32(p, 0x40, 200);                       // below_angle_scale
    for (int i = 0; i < 10; ++i) fx_test::put32(p, 0x44 + (size_t)i * 4, l1);
    fx_test::put32(p, 0x6C, l1 + (uint32_t)kLayerSz);   // colour_entry_table_va
    fx_test::put32(p, 0x70, l1 + (uint32_t)kLayerSz);   // palette_buffer_va
    fx_test::put32(p, 0x74, l0);                        // layer_array_va

    auto fill_layer = [&](size_t base, uint8_t flags, int32_t alt_min,
                          const char* cloud, const char* sky) {
        p[base + 0x00] = flags;
        fx_test::put32(p, base + 0x0A, (uint32_t)alt_min);
        fx_test::put32(p, base + 0x0E, (uint32_t)(alt_min + 5000));
        fx_test::put32(p, base + 0xFE, 7);              // fog_density
        p[base + 0x36] = 10; p[base + 0x37] = 20; p[base + 0x38] = 30;
        for (int j = 0; j < 31; ++j) {                  // zenith ramp
            p[base + 0x3E + (size_t)j * 3] = (uint8_t)j;
            p[base + 0x3F + (size_t)j * 3] = (uint8_t)(j + 1);
            p[base + 0x40 + (size_t)j * 3] = (uint8_t)(j + 2);
        }
        for (int j = 0; j < 32; ++j) {                  // horizon ramp
            p[base + 0x9B + (size_t)j * 3] = (uint8_t)(63 - j);
            p[base + 0x9C + (size_t)j * 3] = (uint8_t)(62 - j);
            p[base + 0x9D + (size_t)j * 3] = (uint8_t)(61 - j);
        }
        memcpy(&p[base + 0x102], cloud, strlen(cloud));
        memcpy(&p[base + 0x118], sky, strlen(sky));
        p[base + 0x14E] = 5;                            // visibility
    };
    fill_layer(kHdr, 0x02, 0, "cloud1.PIC", "day1.PIC");
    fill_layer(kHdr + kLayerSz, 0x01, 20000, "cloud2.PIC", "nite1.PIC");
    return p;
}

std::vector<uint8_t> make_lay() { return fx_test::make_le(lay_payload()); }

}  // namespace

TEST_CASE("lay_parse reads the synthetic header and layer array") {
    auto img = make_lay();
    LayFile lay = lay_parse(img.data(), img.size());
    REQUIRE(lay.valid);
    CHECK(lay.sky_angle_scale == 100u);
    CHECK(lay.below_angle_scale == 200u);
    REQUIRE(lay.layers.size() == 2u);
    CHECK(lay.layers[0].alt_min == 0);
    CHECK(lay.layers[1].alt_min == 20000);
    CHECK(lay.layers[0].cloud_pic == "cloud1.PIC");
    CHECK(lay.layers[1].sky_pic == "nite1.PIC");
    CHECK(lay.layers[0].zenith_grad[3].r == 3);
    CHECK((lay.layers[1].flags & 0x01) == 0x01);
}

TEST_CASE("lay unedited parse-repack is byte-identical") {
    auto img = make_lay();
    LayFile lay = lay_parse(img.data(), img.size());
    REQUIRE(lay.valid);
    auto out = lay_repack(img.data(), img.size(), lay);
    REQUIRE(out == img);
}

TEST_CASE("lay_repack applies header, band, layer, and gradient edits") {
    auto img = make_lay();
    LayFile lay = lay_parse(img.data(), img.size());
    REQUIRE(lay.valid);

    lay.sky_angle_scale = 150;
    lay.sky_layer_va[0] = lay.below_layer_va[0];  // re-point a sky band
    lay.layers[0].fog_density = 12;
    lay.layers[0].cloud_pic = "storm.PIC";
    lay.layers[1].zenith_grad[7] = {1, 2, 3};
    auto out = lay_repack(img.data(), img.size(), lay);
    REQUIRE_FALSE(out.empty());

    LayFile back = lay_parse(out.data(), out.size());
    REQUIRE(back.valid);
    CHECK(back.sky_angle_scale == 150u);
    CHECK(back.sky_layer_va[0] == back.below_layer_va[0]);
    CHECK(back.layers[0].fog_density == 12u);
    CHECK(back.layers[0].cloud_pic == "storm.PIC");
    CHECK(back.layers[1].zenith_grad[7].g == 2);
    CHECK(back.layers[1].alt_min == 20000);  // untouched
}

TEST_CASE("lay_parse and lay_repack reject invalid input") {
    std::vector<uint8_t> junk(64, 0xAB);
    CHECK_FALSE(lay_parse(junk.data(), junk.size()).valid);

    auto tiny = fx_test::make_le({0xC3});
    CHECK_FALSE(lay_parse(tiny.data(), tiny.size()).valid);

    auto img = make_lay();
    LayFile lay = lay_parse(img.data(), img.size());
    REQUIRE(lay.valid);

    // Layer count mismatch.
    LayFile fewer = lay;
    fewer.layers.pop_back();
    CHECK(lay_repack(img.data(), img.size(), fewer).empty());

    // Moving the end sentinel would change the array length.
    LayFile moved = lay;
    moved.layers[0].flags |= 0x01;
    CHECK(lay_repack(img.data(), img.size(), moved).empty());

    // Structural VAs cannot be relocated.
    LayFile reloc = lay;
    reloc.layer_array_va += 4;
    CHECK(lay_repack(img.data(), img.size(), reloc).empty());

    // Picture name longer than its 22-byte slot.
    LayFile long_pic = lay;
    long_pic.layers[0].sky_pic = "a_far_too_long_name.PIC";
    CHECK(lay_repack(img.data(), img.size(), long_pic).empty());
}

TEST_CASE("lay_repack is byte-identical for every install LAY") {
    const char* root = std::getenv("FX_FA_ROOT");
    if (!root || !*root)
        SKIP("FX_FA_ROOT not set (real-asset mode runs on the benches)");
    namespace fs = std::filesystem;
    auto lower = [](std::string s) {
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    int total = 0;
    for (const auto& de : fs::directory_iterator(root)) {
        if (!de.is_regular_file()) continue;
        std::string fn = lower(de.path().filename().string());
        if (fn.size() < 4 || fn.substr(fn.size() - 4) != ".lib") continue;
        std::ifstream f(de.path(), std::ios::binary | std::ios::ate);
        std::vector<uint8_t> lib((size_t)f.tellg());
        f.seekg(0);
        f.read((char*)lib.data(), (std::streamsize)lib.size());

        for (const auto& e : ealib_read_dir(lib.data(), lib.size())) {
            std::string name = lower(e.name);
            if (name.size() < 4 || name.substr(name.size() - 4) != ".lay") continue;
            auto bytes = ealib_extract(lib.data(), lib.size(), e, true);
            INFO(de.path().filename().string() << " / " << e.name);
            REQUIRE_FALSE(bytes.empty());
            LayFile lay = lay_parse(bytes.data(), bytes.size());
            REQUIRE(lay.valid);
            auto out = lay_repack(bytes.data(), bytes.size(), lay);
            REQUIRE(out == bytes);
            ++total;
        }
    }
    REQUIRE(total > 0);
    WARN("LAY repack census: " << total << " overlays byte-identical");
}
