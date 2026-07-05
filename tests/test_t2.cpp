#include <catch2/catch_test_macros.hpp>
#include <fx/t2.h>
#include <cstring>
#include <vector>

using namespace fx;

// Minimal T2 binary in the engine layout: 0x95-byte header (field map in
// fx/t2.h), a flat leaf array (leaves_w*leaves_h x 3 B), then the per-tile
// summary array (tiles_w*tiles_h x 3 B).
static std::vector<uint8_t> make_t2(uint32_t tiles_w, uint32_t tiles_h,
                                     uint8_t surface_class = 0xFF,
                                     uint32_t leaf_step = 8) {
    const uint32_t HEADER      = 0x95;
    uint32_t leaves_w    = tiles_w * leaf_step;
    uint32_t leaves_h    = tiles_h * leaf_step;
    uint32_t leaf_off    = HEADER;
    uint32_t summary_off = leaf_off + leaves_w * leaves_h * 3;
    size_t   total       = summary_off + tiles_w * tiles_h * 3;
    std::vector<uint8_t> buf(total, 0);

    buf[0] = 'B'; buf[1] = 'I'; buf[2] = 'T'; buf[3] = '2';
    auto wr32 = [&](size_t at, uint32_t v) {
        buf[at]     = (uint8_t)(v & 0xFF);
        buf[at + 1] = (uint8_t)((v >> 8) & 0xFF);
        buf[at + 2] = (uint8_t)((v >> 16) & 0xFF);
        buf[at + 3] = (uint8_t)((v >> 24) & 0xFF);
    };
    wr32(0x64, tiles_h);       // legacy duplicate
    wr32(0x79, leaf_step);
    wr32(0x7D, tiles_w);
    wr32(0x81, tiles_h);
    wr32(0x85, summary_off);
    wr32(0x89, leaves_w);
    wr32(0x8D, leaves_h);
    wr32(0x91, leaf_off);

    // Fill every summary record's class byte
    for (uint32_t t = 0; t < tiles_w * tiles_h; t++)
        buf[summary_off + (size_t)t * 3] = surface_class;
    return buf;
}

TEST_CASE("t2_info rejects wrong magic") {
    auto buf  = make_t2(2, 2);
    buf[0] = 'X';
    T2Info info;
    REQUIRE_FALSE(t2_info(buf.data(), buf.size(), &info));
}

TEST_CASE("t2_info rejects truncated buffer") {
    std::vector<uint8_t> tiny(100, 0);
    T2Info info;
    REQUIRE_FALSE(t2_info(tiny.data(), tiny.size(), &info));
}

TEST_CASE("t2_info rejects inconsistent file size") {
    auto buf = make_t2(2, 2);
    buf.push_back(0); // one extra byte breaks the array-extent check
    T2Info info;
    REQUIRE_FALSE(t2_info(buf.data(), buf.size(), &info));
}

TEST_CASE("t2_info rejects a leaf grid that disagrees with the tile grid") {
    auto buf = make_t2(2, 2);
    buf[0x89] = 15; // leaves_w != tiles_w * leaf_step
    T2Info info;
    REQUIRE_FALSE(t2_info(buf.data(), buf.size(), &info));
}

TEST_CASE("t2_info parses 2x2 grid correctly") {
    auto buf = make_t2(2, 2, 0xD2);
    T2Info info;
    REQUIRE(t2_info(buf.data(), buf.size(), &info));
    REQUIRE(info.dim_x      == 2u);
    REQUIRE(info.dim_y      == 2u);
    REQUIRE(info.tile_count == 4u);
    REQUIRE(info.leaf_step  == 8u);
    REQUIRE(info.leaves_w   == 16u);
    REQUIRE(info.leaves_h   == 16u);
    REQUIRE(info.leaf_offset == 0x95u);
    REQUIRE(info.summary_offset == 0x95u + 16u * 16u * 3u);
}

TEST_CASE("t2_info builds surface class distribution from tile summaries") {
    auto buf = make_t2(2, 2, 0xFF);
    T2Info info;
    REQUIRE(t2_info(buf.data(), buf.size(), &info));
    REQUIRE(info.surface_dist.count(0xFF) == 1u);
    REQUIRE(info.surface_dist.at(0xFF) == 4u); // all 4 tiles are water
}

TEST_CASE("t2_info handles 1x1 grid") {
    auto buf = make_t2(1, 1, 0xD0);
    T2Info info;
    REQUIRE(t2_info(buf.data(), buf.size(), &info));
    REQUIRE(info.tile_count == 1u);
    REQUIRE(info.surface_dist.at(0xD0) == 1u);
}

TEST_CASE("t2_info surface_dist sums to tile_count") {
    auto buf = make_t2(3, 3, 0xFF);
    T2Info info;
    REQUIRE(t2_info(buf.data(), buf.size(), &info));
    // Overwrite one summary record's class
    buf[info.summary_offset + 2 * 3] = 0xD2; // tile index 2 is land
    REQUIRE(t2_info(buf.data(), buf.size(), &info));
    uint32_t total = 0;
    for (auto& [k, v] : info.surface_dist) total += v;
    REQUIRE(total == info.tile_count);
    REQUIRE(info.surface_dist.at(0xD2) == 1u);
}
