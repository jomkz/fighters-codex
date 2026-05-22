#include <catch2/catch_test_macros.hpp>
#include <ft/pic.h>
#include <ft/pal.h>
#include <vector>
#include <cstdint>

using namespace ft;

static std::vector<uint8_t> make_rgba(int w, int h, uint8_t r, uint8_t g, uint8_t b) {
    std::vector<uint8_t> buf(w * h * 4);
    for (int i = 0; i < w * h; i++) {
        buf[i*4+0] = r;
        buf[i*4+1] = g;
        buf[i*4+2] = b;
        buf[i*4+3] = 255;
    }
    return buf;
}

TEST_CASE("pic_info rejects empty buffer") {
    uint8_t tiny[1] = {0};
    PicInfo info;
    REQUIRE_FALSE(pic_info(tiny, 0, &info));
}

TEST_CASE("pic_info rejects truncated buffer") {
    uint8_t buf[2] = {0, 0};
    PicInfo info;
    REQUIRE_FALSE(pic_info(buf, 2, &info));
}

TEST_CASE("pic_encode returns non-empty output") {
    auto rgba = make_rgba(8, 8, 200, 100, 50);
    Palette pal = {};
    auto encoded = pic_encode(rgba.data(), 8, 8, pal);
    REQUIRE_FALSE(encoded.empty());
}

TEST_CASE("pic_encode produces a parseable dense header with correct dimensions") {
    auto rgba = make_rgba(16, 16, 200, 100, 50);
    Palette pal = {};
    auto encoded = pic_encode(rgba.data(), 16, 16, pal);
    REQUIRE_FALSE(encoded.empty());

    PicInfo info;
    REQUIRE(pic_info(encoded.data(), encoded.size(), &info));
    REQUIRE(info.width  == 16u);
    REQUIRE(info.height == 16u);
    REQUIRE(info.format == 0u); // dense format
}

TEST_CASE("pic_encode / pic_decode round-trip produces correct buffer size") {
    auto rgba = make_rgba(8, 8, 128, 64, 255);
    Palette pal = {};
    auto encoded = pic_encode(rgba.data(), 8, 8, pal);
    REQUIRE_FALSE(encoded.empty());

    auto decoded = pic_decode(encoded.data(), encoded.size(), nullptr);
    REQUIRE(decoded.size() == 8u * 8u * 4u);
}

TEST_CASE("pic_decode on encoded solid-color image returns opaque pixels") {
    auto rgba = make_rgba(4, 4, 0, 0, 255); // solid blue
    Palette pal = {};
    auto encoded = pic_encode(rgba.data(), 4, 4, pal);
    REQUIRE_FALSE(encoded.empty());

    auto decoded = pic_decode(encoded.data(), encoded.size(), nullptr);
    REQUIRE(decoded.size() == 4u * 4u * 4u);
    // All pixels should be fully opaque (alpha = 255)
    for (int i = 3; i < (int)decoded.size(); i += 4)
        REQUIRE(decoded[i] == 255);
}

TEST_CASE("pic_decode on empty data returns empty vector") {
    uint8_t tiny[1] = {0};
    auto decoded = pic_decode(tiny, 0, nullptr);
    REQUIRE(decoded.empty());
}
