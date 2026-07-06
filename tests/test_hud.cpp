// HUD write path — parse→repack byte-identity and guarded edits (#99).
#include <catch2/catch_test_macros.hpp>
#include <fx/ealib.h>
#include <fx/hud.h>
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

// A synthetic HUD CODE payload: exactly the fixed 0x2BB-byte struct with
// asset strings, gauge parameters, and the four advisory icon slots laid
// out where hud_parse reads them.
std::vector<uint8_t> hud_payload() {
    std::vector<uint8_t> p(0x2BB, 0);
    auto put_str = [&](size_t off, const char* s) {
        memcpy(&p[off], s, strlen(s));  // regions are pre-zeroed
    };
    put_str(0x01, "~a7");
    put_str(0x08, "hudsym");
    // Gauge params: distinctive values at a few offsets hud_parse models.
    fx_test::put16(p, 0x1E1, 120);              // heading_tape.width
    fx_test::put16(p, 0x1F7, (uint16_t)-24);    // speed_tape.dx (s16)
    p[0x23B] = 1;                               // hud.center_dot_enable (u8)
    p[0x241] = (uint8_t)(int8_t)-5;             // score_indicator.dx (s8)
    put_str(0x245, "GEAR");
    put_str(0x24D, "FLAP");
    put_str(0x255, "BRAKE");
    put_str(0x25D, "HOOK");
    put_str(0x275, "~a7s");
    return p;
}

std::vector<uint8_t> make_hud() { return fx_test::make_le(hud_payload()); }

int16_t param(const HudFile& h, const char* gauge, const char* field) {
    for (const auto& p : h.params)
        if (p.gauge == gauge && p.field == field) return p.value;
    FAIL("param not found: " << gauge << "." << field);
    return 0;
}

void set_param(HudFile& h, const char* gauge, const char* field, int16_t v) {
    for (auto& p : h.params)
        if (p.gauge == gauge && p.field == field) { p.value = v; return; }
    FAIL("param not found: " << gauge << "." << field);
}

}  // namespace

TEST_CASE("hud_parse reads the synthetic struct") {
    auto img = make_hud();
    HudFile hud = hud_parse(img.data(), img.size());
    REQUIRE(hud.valid);
    CHECK(param(hud, "heading_tape", "width") == 120);
    CHECK(param(hud, "speed_tape", "dx") == -24);
    CHECK(param(hud, "hud", "center_dot_enable") == 1);
    CHECK(param(hud, "score_indicator", "dx") == -5);
    CHECK(hud.icon_a == "GEAR");
    CHECK(hud.icon_d == "HOOK");
    REQUIRE(hud.asset_strings.size() >= 2u);
    CHECK(hud.asset_strings[0] == "~a7");
}

TEST_CASE("hud unedited parse-repack is byte-identical") {
    auto img = make_hud();
    HudFile hud = hud_parse(img.data(), img.size());
    REQUIRE(hud.valid);
    auto out = hud_repack(img.data(), img.size(), hud);
    REQUIRE(out == img);
}

TEST_CASE("hud_repack applies param and icon edits") {
    auto img = make_hud();
    HudFile hud = hud_parse(img.data(), img.size());
    REQUIRE(hud.valid);

    set_param(hud, "speed_tape", "dx", -48);
    set_param(hud, "hud", "center_dot_enable", 0);
    hud.icon_d = "BAY";
    auto out = hud_repack(img.data(), img.size(), hud);
    REQUIRE_FALSE(out.empty());

    HudFile back = hud_parse(out.data(), out.size());
    REQUIRE(back.valid);
    CHECK(param(back, "speed_tape", "dx") == -48);
    CHECK(param(back, "hud", "center_dot_enable") == 0);
    CHECK(back.icon_d == "BAY");
    CHECK(param(back, "heading_tape", "width") == 120);  // untouched
}

TEST_CASE("hud_parse and hud_repack reject invalid input") {
    std::vector<uint8_t> junk(64, 0xAB);
    CHECK_FALSE(hud_parse(junk.data(), junk.size()).valid);

    // CODE section smaller than the fixed struct.
    auto tiny = fx_test::make_le({0xC3});
    CHECK_FALSE(hud_parse(tiny.data(), tiny.size()).valid);

    auto img = make_hud();
    HudFile hud = hud_parse(img.data(), img.size());
    REQUIRE(hud.valid);

    // Icon label longer than its 8-byte slot.
    HudFile long_icon = hud;
    long_icon.icon_a = "TOOLONGXX";
    CHECK(hud_repack(img.data(), img.size(), long_icon).empty());

    // u8 param out of range.
    HudFile bad_u8 = hud;
    set_param(bad_u8, "hud", "center_dot_enable", 300);
    CHECK(hud_repack(img.data(), img.size(), bad_u8).empty());

    // s8 param out of range.
    HudFile bad_s8 = hud;
    set_param(bad_s8, "score_indicator", "dx", 200);
    CHECK(hud_repack(img.data(), img.size(), bad_s8).empty());

    // Missing and renamed params reject rather than silently no-op.
    HudFile missing = hud;
    missing.params.pop_back();
    CHECK(hud_repack(img.data(), img.size(), missing).empty());
    HudFile unknown = hud;
    unknown.params[0].field = "no_such_field";
    CHECK(hud_repack(img.data(), img.size(), unknown).empty());
}

TEST_CASE("hud_repack is byte-identical for every install HUD") {
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
            if (name.size() < 4 || name.substr(name.size() - 4) != ".hud") continue;
            auto bytes = ealib_extract(lib.data(), lib.size(), e, true);
            INFO(de.path().filename().string() << " / " << e.name);
            REQUIRE_FALSE(bytes.empty());
            HudFile hud = hud_parse(bytes.data(), bytes.size());
            REQUIRE(hud.valid);
            auto out = hud_repack(bytes.data(), bytes.size(), hud);
            REQUIRE(out == bytes);
            ++total;
        }
    }
    REQUIRE(total > 0);
    WARN("HUD repack census: " << total << " overlays byte-identical");
}
