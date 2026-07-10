#include <catch2/catch_test_macros.hpp>
#include <fx/effect.h>
#include <cstring>
#include <vector>

using namespace fx;

// A synthetic 0x30-byte effect-parameter record with distinctive little-endian
// field values, so any misplaced offset breaks a check.
static std::vector<uint8_t> make_record() {
    std::vector<uint8_t> r(EFFECT_RECORD_SIZE, 0);
    auto w16 = [&](size_t off, uint16_t v) {
        r[off] = (uint8_t)v; r[off + 1] = (uint8_t)(v >> 8);
    };
    auto w32 = [&](size_t off, uint32_t v) {
        r[off] = (uint8_t)v; r[off + 1] = (uint8_t)(v >> 8);
        r[off + 2] = (uint8_t)(v >> 16); r[off + 3] = (uint8_t)(v >> 24);
    };
    w16(0x04, 0x0050);        // intensity = 80
    w16(0x06, 0x0003);        // frame_count = 3
    w16(0x08, 0x0104);        // subtype = 0x0104 (low byte 0x04 -> ground_burst)
    w16(0x0A, 0x0006);        // debris_count = 6
    w16(0x0C, 0x0020);        // debris_spread = 32
    w32(0x0E, 0x004F1000);    // sound ptr 0
    w32(0x12, 0x004F1010);    // sound ptr 1
    w32(0x16, 0x00000000);    // terminator -> 2 sound variants
    w32(0x1A, 0x004F1020);    // present but after the null: must NOT be counted
    w16(0x2E, 0x001E);        // sound_pitch = 30
    return r;
}

TEST_CASE("effect_class_for_type maps the GRAPHICInit ranges") {
    REQUIRE(effect_class_for_type(0)    == EffectClass::None);
    REQUIRE(effect_class_for_type(1)    == EffectClass::Crater);
    REQUIRE(effect_class_for_type(3)    == EffectClass::Crater);
    REQUIRE(effect_class_for_type(4)    == EffectClass::Debris);
    REQUIRE(effect_class_for_type(7)    == EffectClass::Smoke);
    REQUIRE(effect_class_for_type(11)   == EffectClass::Smoke);
    REQUIRE(effect_class_for_type(12)   == EffectClass::Chaff);
    REQUIRE(effect_class_for_type(13)   == EffectClass::Flare);
    REQUIRE(effect_class_for_type(14)   == EffectClass::Fire);
    REQUIRE(effect_class_for_type(15)   == EffectClass::Explosion);
    REQUIRE(effect_class_for_type(0x26) == EffectClass::Explosion);
    REQUIRE(effect_class_for_type(0x27) == EffectClass::Unknown);   // gap
    REQUIRE(effect_class_for_type(0x28) == EffectClass::DustPuff);
    REQUIRE(effect_class_for_type(0x2A) == EffectClass::DustPuff);
    REQUIRE(effect_class_for_type(0x2B) == EffectClass::Unknown);   // out of range
    REQUIRE(effect_class_for_type(-1)   == EffectClass::Unknown);
}

TEST_CASE("effect_shape_for_type names the .SH shapes") {
    REQUIRE(std::string(effect_shape_for_type(1))    == "crater.SH");
    REQUIRE(std::string(effect_shape_for_type(14))   == "fire.SH");
    REQUIRE(std::string(effect_shape_for_type(0x15)) == "exp.SH");
    REQUIRE(std::string(effect_shape_for_type(0x28)) == "spd.SH");
    REQUIRE(std::string(effect_shape_for_type(0x29)) == "mpd.SH");
    REQUIRE(std::string(effect_shape_for_type(0x2A)) == "lpd.SH");
    REQUIRE(std::string(effect_shape_for_type(0))    == "");       // none
    REQUIRE(std::string(effect_shape_for_type(0x27)) == "");       // unknown
}

TEST_CASE("effect_parse_record maps documented field offsets") {
    auto r = make_record();
    EffectParams p;
    // Record for type 15 (explosion) lives at index 15 in a full table; a
    // single-record buffer is the type-0 slot.
    REQUIRE(effect_parse_record(r.data(), r.size(), 0, p));
    REQUIRE(p.type          == 0);
    REQUIRE(p.klass         == EffectClass::None);
    REQUIRE(p.intensity     == 80);
    REQUIRE(p.frame_count   == 3);
    REQUIRE(p.subtype       == 0x0104);
    REQUIRE(p.ground_burst  == true);
    REQUIRE(p.debris_count  == 6);
    REQUIRE(p.debris_spread == 32);
    REQUIRE(p.sound_pitch   == 30);
    REQUIRE(p.sound_variants == 2);              // stops at the +0x16 null
    REQUIRE(p.sound_ptrs[0] == 0x004F1000);
    REQUIRE(p.sound_ptrs[1] == 0x004F1010);
}

TEST_CASE("effect_parse_record indexes by type and bounds-checks") {
    // Two records back to back; type 1 must read the SECOND record's fields.
    auto a = make_record();
    auto b = make_record();
    b[0x04] = 0x11; b[0x05] = 0x00;   // second record intensity = 0x11
    std::vector<uint8_t> table;
    table.insert(table.end(), a.begin(), a.end());
    table.insert(table.end(), b.begin(), b.end());

    EffectParams p;
    REQUIRE(effect_parse_record(table.data(), table.size(), 1, p));
    REQUIRE(p.type      == 1);
    REQUIRE(p.klass     == EffectClass::Crater);
    REQUIRE(p.intensity == 0x11);

    SECTION("type past the end is rejected") {
        REQUIRE_FALSE(effect_parse_record(table.data(), table.size(), 2, p));
    }
    SECTION("short buffer is rejected") {
        REQUIRE_FALSE(effect_parse_record(a.data(), a.size() - 1, 0, p));
    }
}

TEST_CASE("effect_parse_table stops when the buffer runs out") {
    auto a = make_record();
    std::vector<uint8_t> table;
    for (int i = 0; i < 3; i++) table.insert(table.end(), a.begin(), a.end());
    auto recs = effect_parse_table(table.data(), table.size(), 10);
    REQUIRE(recs.size() == 3);            // only 3 whole records present
    REQUIRE(recs[2].type == 2);
}

TEST_CASE("effect_parse_spawn maps the MSG 0x8003 record") {
    std::vector<uint8_t> s(EFFECT_SPAWN_SIZE, 0);
    s[0x00] = 0x12;                                        // type
    s[0x01] = 0x00; s[0x02] = 0x01; s[0x03] = 0; s[0x04] = 0;  // x = 0x100 = 1.0 ft
    s[0x05] = 0x00; s[0x06] = 0x02; s[0x07] = 0; s[0x08] = 0;  // y = 0x200 = 2.0 ft
    s[0x09] = 0x00; s[0x0A] = 0xFF; s[0x0B] = 0xFF; s[0x0C] = 0xFF;  // z = -0x100
    s[0x0D] = 0x34; s[0x0E] = 0x12;                        // owner = 0x1234
    s[0x0F] = 0xAB; s[0x10] = 0xCD;                        // flags

    EffectSpawn out;
    REQUIRE(effect_parse_spawn(s.data(), s.size(), out));
    REQUIRE(out.type  == 0x12);
    REQUIRE(out.x     == 0x100);
    REQUIRE(out.y     == 0x200);
    REQUIRE(out.z     == -0x100);
    REQUIRE(out.owner == 0x1234);
    REQUIRE(out.flag0 == 0xAB);
    REQUIRE(out.flag1 == 0xCD);

    REQUIRE_FALSE(effect_parse_spawn(s.data(), s.size() - 1, out));  // too short
}
