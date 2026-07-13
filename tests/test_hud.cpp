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
    // The tape fields are FOUR bytes, sign-extended, exactly as every shipped HUD stores
    // them. They used to be written here with put16 — a fixture that encoded the codec's
    // own mistake and then defended it, which is how the 16-bit write bug survived (#491).
    fx_test::put32(p, 0x1E1, 120);              // heading_tape.width (i32)
    fx_test::put32(p, 0x1F7, (uint32_t)-24);    // speed_tape.dx (i32, negative)
    fx_test::put16(p, 0x265, (uint16_t)-38);    // warning_lights.dx (genuinely s16)
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

// Where make_le() puts the CODE section's raw bytes: e_lfanew (0x40) + 24 + one 40-byte
// section entry. Lets a test assert the bytes on disk, not just the round-trip.
constexpr size_t kCodeRaw = 0x40 + 24 + 40;

// int32_t, not int16_t: the tape gauges are 32-bit, and a 16-bit helper here would
// truncate the very values these tests exist to check (#491).
int32_t param(const HudFile& h, const char* gauge, const char* field) {
    for (const auto& p : h.params)
        if (p.gauge == gauge && p.field == field) return p.value;
    FAIL("param not found: " << gauge << "." << field);
    return 0;
}

void set_param(HudFile& h, const char* gauge, const char* field, int32_t v) {
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

// The test that was missing (#491). An unedited repack rewrites the bytes it just read, so
// it is byte-identical whether or not the codec understands the field's WIDTH. Only an edit
// that changes the sign can see a stale high half: writing 40 over -24 as two bytes leaves
// `28 00 ff ff` behind, which the engine reads as -65496.
TEST_CASE("hud edit writes every byte of a 32-bit tape field") {
    auto img = make_hud();
    HudFile hud = hud_parse(img.data(), img.size());
    REQUIRE(hud.valid);
    REQUIRE(param(hud, "speed_tape", "dx") == -24);   // negative: high half is ff ff

    set_param(hud, "speed_tape", "dx", 40);           // sign change
    auto out = hud_repack(img.data(), img.size(), hud);
    REQUIRE_FALSE(out.empty());

    HudFile back = hud_parse(out.data(), out.size());
    REQUIRE(back.valid);
    CHECK(param(back, "speed_tape", "dx") == 40);

    // Read the bytes, not just the round-trip: the high half must be cleared, not stale.
    const uint8_t* cs = out.data() + kCodeRaw;
    CHECK(cs[0x1F7] == 0x28);
    CHECK(cs[0x1F8] == 0x00);
    CHECK(cs[0x1F9] == 0x00);   // was 0xFF before the edit
    CHECK(cs[0x1FA] == 0x00);   // was 0xFF before the edit

    // And the reverse: a positive field edited negative must sign-extend all four bytes.
    HudFile neg = hud_parse(img.data(), img.size());
    set_param(neg, "heading_tape", "width", -1);
    auto out2 = hud_repack(img.data(), img.size(), neg);
    REQUIRE_FALSE(out2.empty());
    const uint8_t* cs2 = out2.data() + kCodeRaw;
    CHECK(cs2[0x1E1] == 0xFF);
    CHECK(cs2[0x1E2] == 0xFF);
    CHECK(cs2[0x1E3] == 0xFF);
    CHECK(cs2[0x1E4] == 0xFF);
    CHECK(param(hud_parse(out2.data(), out2.size()), "heading_tape", "width") == -1);

    // A 32-bit field must also carry a value no 16-bit field could hold.
    HudFile big = hud_parse(img.data(), img.size());
    set_param(big, "altitude_tape", "height", 100000);
    auto out3 = hud_repack(img.data(), img.size(), big);
    REQUIRE_FALSE(out3.empty());
    CHECK(param(hud_parse(out3.data(), out3.size()), "altitude_tape", "height") == 100000);

    // The genuinely 16-bit fields stay 16-bit: a value that overflows them is rejected,
    // never silently truncated into the neighbouring field two bytes along.
    HudFile bad = hud_parse(img.data(), img.size());
    set_param(bad, "warning_lights", "dx", 100000);
    CHECK(hud_repack(img.data(), img.size(), bad).empty());
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

            // An unedited repack rewrites the bytes it read, so it cannot see a wrong field
            // WIDTH. Every field the editor can write gets written here, with the sign
            // flipped — the edit `fx hud set` actually performs (#491).
            for (const auto& p : hud.params) {
                HudFile edited = hud;
                const int32_t original = p.value;
                const int32_t flipped  = (original > 0) ? -original
                                       : (original < 0) ? -original
                                                        : 1;
                for (auto& q : edited.params)
                    if (q.gauge == p.gauge && q.field == p.field) q.value = flipped;

                auto mod = hud_repack(bytes.data(), bytes.size(), edited);
                INFO(e.name << " " << p.gauge << "." << p.field
                            << ": " << original << " -> " << flipped);
                if (mod.empty()) continue;   // u8/s8 fields legitimately reject a negative

                HudFile back = hud_parse(mod.data(), mod.size());
                REQUIRE(back.valid);
                bool seen = false;
                for (const auto& q : back.params)
                    if (q.gauge == p.gauge && q.field == p.field) {
                        CHECK(q.value == flipped);   // the whole value, not just its low half
                        seen = true;
                    }
                CHECK(seen);
                // Nothing but this field may move: a too-wide write would spill into the
                // neighbouring field, and a too-narrow one would leave a stale high half.
                size_t differing = 0;
                for (size_t i = 0; i < bytes.size(); ++i)
                    if (mod[i] != bytes[i]) ++differing;
                CHECK(differing <= 4u);
            }
            ++total;
        }
    }
    REQUIRE(total > 0);
    WARN("HUD repack census: " << total << " overlays byte-identical");
}
