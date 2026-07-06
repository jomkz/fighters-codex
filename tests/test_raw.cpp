// RAW screenshot codec — writer + structural repack (#96).
#include <catch2/catch_test_macros.hpp>
#include <fx/raw.h>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace fx;

TEST_CASE("raw_encode -> raw_decode round-trips RGBA pixel-identically") {
    // 8x4 with three colours and a gradient-ish layout.
    const int w = 8, h = 4;
    std::vector<uint8_t> rgba((size_t)w * h * 4);
    for (int i = 0; i < w * h; i++) {
        const int c = i % 3;
        rgba[i*4+0] = (uint8_t)(c == 0 ? 200 : 10);
        rgba[i*4+1] = (uint8_t)(c == 1 ? 150 : 20);
        rgba[i*4+2] = (uint8_t)(c == 2 ? 90 : 30);
        rgba[i*4+3] = 255;
    }
    auto raw = raw_encode(rgba.data(), w, h);
    REQUIRE_FALSE(raw.empty());
    REQUIRE(raw.size() == 800u + (size_t)w * h);

    RawInfo info;
    REQUIRE(raw_info(raw.data(), raw.size(), &info));
    REQUIRE(info.width == (uint32_t)w);
    REQUIRE(info.height == (uint32_t)h);
    // Header constants match the engine's own captures.
    CHECK(raw[6] == 0x00);
    CHECK(raw[7] == 0x04);
    CHECK(raw[12] == 0x01);
    CHECK(raw[13] == 0x00);

    auto back = raw_decode(raw.data(), raw.size());
    REQUIRE(back == rgba);
}

TEST_CASE("raw_encode fails past 256 distinct colours") {
    const int w = 32, h = 16;  // 512 pixels, all distinct colours
    std::vector<uint8_t> rgba((size_t)w * h * 4);
    for (int i = 0; i < w * h; i++) {
        rgba[i*4+0] = (uint8_t)(i & 0xFF);
        rgba[i*4+1] = (uint8_t)(i >> 8);
        rgba[i*4+2] = 0;
        rgba[i*4+3] = 255;
    }
    CHECK(raw_encode(rgba.data(), w, h).empty());
}

TEST_CASE("raw_repack is byte-identical and rejects trailing bytes") {
    const int w = 4, h = 4;
    std::vector<uint8_t> rgba((size_t)w * h * 4, 128);
    auto raw = raw_encode(rgba.data(), w, h);
    REQUIRE_FALSE(raw.empty());

    auto out = raw_repack(raw.data(), raw.size());
    REQUIRE(out == raw);

    raw.push_back(0);  // a byte the layout does not describe
    CHECK(raw_repack(raw.data(), raw.size()).empty());
}

TEST_CASE("raw_repack is byte-identical for the engine's own captures") {
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
        if (lower(de.path().extension().string()) != ".raw") continue;
        std::ifstream f(de.path(), std::ios::binary | std::ios::ate);
        std::vector<uint8_t> data((size_t)f.tellg());
        f.seekg(0);
        f.read((char*)data.data(), (std::streamsize)data.size());

        INFO(de.path().filename().string());
        RawInfo info;
        REQUIRE(raw_info(data.data(), data.size(), &info));
        auto out = raw_repack(data.data(), data.size());
        REQUIRE(out == data);
        // The decode -> encode -> decode loop is pixel-exact too.
        auto rgba = raw_decode(data.data(), data.size());
        REQUIRE_FALSE(rgba.empty());
        auto re = raw_encode(rgba.data(), (int)info.width, (int)info.height);
        REQUIRE_FALSE(re.empty());
        REQUIRE(raw_decode(re.data(), re.size()) == rgba);
        ++total;
    }
    if (total == 0) SKIP("no screen*.raw captures in this install");
    WARN("RAW repack census: " << total << " captures byte-identical");
}
