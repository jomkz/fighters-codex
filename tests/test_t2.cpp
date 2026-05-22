#include <catch2/catch_test_macros.hpp>
#include <fx/t2.h>
#include <cstring>
#include <vector>

using namespace fx;

// Minimal T2 binary: 149-byte header + dim_x*dim_y*195 bytes of tile data.
static std::vector<uint8_t> make_t2(uint32_t dim_x, uint8_t dim_y,
                                     uint8_t surface_class = 0xFF) {
    const uint32_t HEADER = 149;
    const uint32_t TILE   = 195;
    size_t total = HEADER + (size_t)dim_x * dim_y * TILE;
    std::vector<uint8_t> buf(total, 0);

    // Magic
    buf[0] = 'B'; buf[1] = 'I'; buf[2] = 'T'; buf[3] = '2';
    // dim_x at 0x64 (u32 LE)
    buf[0x64] = (uint8_t)(dim_x & 0xFF);
    buf[0x65] = (uint8_t)((dim_x >> 8) & 0xFF);
    buf[0x66] = (uint8_t)((dim_x >> 16) & 0xFF);
    buf[0x67] = (uint8_t)((dim_x >> 24) & 0xFF);
    // dim_y at 0x7D (u8)
    buf[0x7D] = dim_y;

    // Fill record 0 byte 0 of each tile with the surface class
    for (uint32_t t = 0; t < (uint32_t)dim_x * dim_y; t++) {
        size_t tile_base = HEADER + (size_t)t * TILE;
        buf[tile_base] = surface_class;
    }
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
    buf.push_back(0); // one extra byte breaks the size check
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
    REQUIRE(info.tile_offset == 149u);
}

TEST_CASE("t2_info builds surface class distribution") {
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
    // Overwrite one tile's surface class
    const size_t HEADER = 149, TILE = 195;
    buf[HEADER + 2 * TILE] = 0xD2; // tile index 2 is land
    T2Info info;
    REQUIRE(t2_info(buf.data(), buf.size(), &info));
    uint32_t total = 0;
    for (auto& [k, v] : info.surface_dist) total += v;
    REQUIRE(total == info.tile_count);
}
