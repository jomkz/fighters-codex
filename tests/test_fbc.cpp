#include <catch2/catch_test_macros.hpp>
#include <fx/fbc.h>

using namespace fx;

TEST_CASE("fbc_read parses a flat u32le frame-size array") {
    // Three frames: 100, 256, 65536 bytes.
    std::vector<uint8_t> data = {
        100, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
    };
    bool ok = false;
    auto sizes = fbc_read(data.data(), data.size(), &ok);
    REQUIRE(ok);
    REQUIRE(sizes == std::vector<uint32_t>{ 100, 256, 65536 });
}

TEST_CASE("fbc_read rejects a size that is not a multiple of 4") {
    std::vector<uint8_t> data = { 1, 2, 3 };
    bool ok = true;
    REQUIRE(fbc_read(data.data(), data.size(), &ok).empty());
    REQUIRE_FALSE(ok);
}

TEST_CASE("fbc_read accepts an empty (zero-frame) index") {
    bool ok = false;
    REQUIRE(fbc_read(nullptr, 0, &ok).empty());
    REQUIRE(ok);
}

TEST_CASE("fbc_write is the byte-identical inverse of fbc_read") {
    std::vector<uint8_t> data = {
        0x01, 0x02, 0x03, 0x04,
        0xFF, 0xFF, 0xFF, 0xFF,
        0x00, 0x00, 0x00, 0x00,
    };
    auto sizes = fbc_read(data.data(), data.size());
    REQUIRE(fbc_write(sizes) == data);
}

TEST_CASE("fbc_frame_offset accumulates from the 816-byte VDO header") {
    // FBC.md invariant: frame_data_offset(n) = 816 + sum(FBC[0..n)), and
    // offset(N) equals the paired VDO's file size.
    std::vector<uint32_t> sizes = { 10, 20, 30 };
    REQUIRE(fbc_frame_offset(sizes, 0) == 816);
    REQUIRE(fbc_frame_offset(sizes, 1) == 826);
    REQUIRE(fbc_frame_offset(sizes, 2) == 846);
    REQUIRE(fbc_frame_offset(sizes, 3) == 876);
    REQUIRE(fbc_frame_offset(sizes, 99) == 876); // clamped to N
}
