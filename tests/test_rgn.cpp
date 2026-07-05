#include <catch2/catch_test_macros.hpp>
#include <fx/rgn.h>
#include <cstring>
#include <vector>

using namespace fx;

static void put32(std::vector<uint8_t>& b, size_t off, uint32_t v) {
    b[off] = (uint8_t)v;             b[off + 1] = (uint8_t)(v >> 8);
    b[off + 2] = (uint8_t)(v >> 16); b[off + 3] = (uint8_t)(v >> 24);
}

// Two records shaped like POSTER.RGN's documented B1/B2 (RGN.md).
static std::vector<uint8_t> make_rgn() {
    std::vector<uint8_t> b(4 + 2 * 40, 0);
    put32(b, 0, 2);
    memcpy(&b[4], "B1", 2);
    put32(b, 8, 4);  // vertex count
    uint32_t b1[8] = { 0, 328, 128, 328, 128, 366, 0, 366 };
    for (int k = 0; k < 8; k++) put32(b, 12 + k * 4, b1[k]);
    memcpy(&b[44], "NE1", 3);
    put32(b, 48, 4);
    uint32_t b2[8] = { 128, 328, 340, 328, 340, 366, 128, 366 };
    for (int k = 0; k < 8; k++) put32(b, 52 + k * 4, b2[k]);
    return b;
}

TEST_CASE("rgn_read parses records and names") {
    auto b = make_rgn();
    RgnFile rgn;
    REQUIRE(rgn_read(b.data(), b.size(), rgn));
    REQUIRE(rgn.records.size() == 2);
    REQUIRE(rgn_name(rgn.records[0]) == "B1");
    REQUIRE(rgn_name(rgn.records[1]) == "NE1");
    REQUIRE(rgn.records[0].vertex_count == 4);
    REQUIRE(rgn.records[0].xy[1] == 328);
    REQUIRE(rgn.records[1].xy[2] == 340);
}

TEST_CASE("rgn_read enforces the size invariant") {
    auto b = make_rgn();
    RgnFile rgn;
    REQUIRE_FALSE(rgn_read(b.data(), b.size() - 1, rgn));  // truncated record
    REQUIRE_FALSE(rgn_read(b.data(), 3, rgn));             // shorter than count
    put32(b, 0, 3);                                        // count lies
    REQUIRE_FALSE(rgn_read(b.data(), b.size(), rgn));
}

TEST_CASE("rgn_read accepts an empty region map") {
    std::vector<uint8_t> b(4, 0);
    RgnFile rgn;
    REQUIRE(rgn_read(b.data(), b.size(), rgn));
    REQUIRE(rgn.records.empty());
}

TEST_CASE("rgn_write is the byte-identical inverse of rgn_read") {
    auto b = make_rgn();
    RgnFile rgn;
    REQUIRE(rgn_read(b.data(), b.size(), rgn));
    REQUIRE(rgn_write(rgn) == b);
}
