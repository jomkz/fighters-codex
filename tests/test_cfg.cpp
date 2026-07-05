#include <catch2/catch_test_macros.hpp>
#include <fx/cfg.h>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace fx;

// Synthetic 347-byte EA.CFG with distinctive bytes everywhere, so any
// misplaced field boundary breaks the round-trip.
static std::vector<uint8_t> make_cfg() {
    std::vector<uint8_t> b(EA_CFG_SIZE);
    for (size_t i = 0; i < b.size(); i++) b[i] = (uint8_t)(i * 7 + 3);
    b[0] = 0x24; b[1] = b[2] = b[3] = 0;  // magic dword
    return b;
}

TEST_CASE("cfg_read validates the engine's own checks") {
    auto b = make_cfg();
    EaCfg c;
    REQUIRE(cfg_read(b.data(), b.size(), c));

    SECTION("wrong size") {
        REQUIRE_FALSE(cfg_read(b.data(), b.size() - 1, c));
    }
    SECTION("wrong magic") {
        b[0] = 0x25;
        REQUIRE_FALSE(cfg_read(b.data(), b.size(), c));
    }
}

TEST_CASE("cfg_read maps documented field offsets") {
    auto b = make_cfg();
    b[0x0C] = 2; b[0x0D] = b[0x0E] = b[0x0F] = 0;   // stick_device = 2
    b[0xE3] = 1;                                     // sound_on
    b[0xE5] = 0x34; b[0xE6] = 0x12;                  // overall_vol = 0x1234
    memcpy(&b[0x108], "MAVERICK", 9);                // campaign pilot
    EaCfg c;
    REQUIRE(cfg_read(b.data(), b.size(), c));
    REQUIRE(c.stick_device == 2);
    REQUIRE(c.sound_on == 1);
    REQUIRE(c.overall_vol == 0x1234);
    REQUIRE(memcmp(c.campaign_pilot, "MAVERICK", 9) == 0);
}

TEST_CASE("cfg_write is the byte-identical inverse of cfg_read") {
    auto b = make_cfg();
    EaCfg c;
    REQUIRE(cfg_read(b.data(), b.size(), c));
    REQUIRE(cfg_write(c) == b);
}

TEST_CASE("cfg round-trips the real EA.CFG from the FA install") {
    const char* root = std::getenv("FX_FA_ROOT");
    if (!root || !*root)
        SKIP("FX_FA_ROOT not set (real-asset mode runs on the benches)");
    std::filesystem::path p = std::filesystem::path(root) / "EA.CFG";
    if (!std::filesystem::is_regular_file(p))
        SKIP("EA.CFG not present in this install");

    std::ifstream f(p, std::ios::binary | std::ios::ate);
    std::vector<uint8_t> data((size_t)f.tellg());
    f.seekg(0);
    f.read((char*)data.data(), (std::streamsize)data.size());

    EaCfg c;
    REQUIRE(cfg_read(data.data(), data.size(), c));
    REQUIRE(c.magic == EA_CFG_MAGIC);
    REQUIRE(cfg_write(c) == data);
}
