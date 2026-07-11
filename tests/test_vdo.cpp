#include <catch2/catch_test_macros.hpp>
#include <fx/vdo.h>
#include <cstdint>
#include <vector>

using namespace fx;

namespace {

void put16(std::vector<uint8_t>& v, size_t off, uint16_t x) {
    v[off] = (uint8_t)(x & 0xff);
    v[off + 1] = (uint8_t)(x >> 8);
}

// An 816-byte VDO header for a w x h movie, palette index i -> grey (i,i,i)6.
std::vector<uint8_t> make_header(uint16_t w, uint16_t h) {
    std::vector<uint8_t> v(816, 0);
    v[0] = 'R'; v[1] = 'A'; v[2] = 'T'; v[3] = 'V'; v[4] = 'I'; v[5] = 'D';
    v[6] = 1; v[7] = 2;
    put16(v, 0x08, 15);           // fps (low half)
    put16(v, 0x12, w);
    put16(v, 0x14, h);
    put16(v, 0x16, 256);
    put16(v, 0x18, 1);
    put16(v, 0x1a, 8000);
    for (int i = 0; i < 256; i++) {
        v[0x30 + i * 3 + 0] = (uint8_t)(i >> 2);  // 6-bit grey
        v[0x30 + i * 3 + 1] = (uint8_t)(i >> 2);
        v[0x30 + i * 3 + 2] = (uint8_t)(i >> 2);
    }
    return v;
}

// FBC = flat u32le array of frame sizes.
std::vector<uint8_t> make_fbc(const std::vector<uint32_t>& sizes) {
    std::vector<uint8_t> v;
    for (uint32_t s : sizes)
        for (int b = 0; b < 4; b++) v.push_back((uint8_t)(s >> (8 * b)));
    return v;
}

// A tag-1 whole-canvas RLE frame: fill all `count` pixels with `val`.
std::vector<uint8_t> frame_fill(uint16_t count, uint8_t val) {
    std::vector<uint8_t> f(6, 0);
    put16(f, 0, 1);          // tag = 1 (whole-canvas refresh)
    put16(f, 2, count);      // RLE output count
    // f[4..5] = discarded u16
    f.push_back((uint8_t)(0x80 | ((count - 1) & 0x7f)));  // run of count (<=128)
    f.push_back(val);
    return f;
}

} // namespace

TEST_CASE("vdo_info validates the RATVID header") {
    auto v = make_header(8, 8);
    VdoInfo info{};
    REQUIRE(vdo_info(v.data(), v.size(), &info));
    REQUIRE(info.width == 8);
    REQUIRE(info.height == 8);
    REQUIRE(info.audio_hz == 8000);

    v[0] = 'X';  // break the magic
    REQUIRE_FALSE(vdo_info(v.data(), v.size(), &info));
}

TEST_CASE("vdo_open requires the FBC to tile the frame region") {
    auto v = make_header(8, 8);
    auto f0 = frame_fill(64, 7);
    v.insert(v.end(), f0.begin(), f0.end());

    auto fbc_good = make_fbc({ (uint32_t)f0.size() });
    auto fbc_bad = make_fbc({ (uint32_t)f0.size() + 1 });

    VdoDecoder* d = vdo_open(v.data(), v.size(), fbc_good.data(), fbc_good.size());
    REQUIRE(d != nullptr);
    vdo_close(d);
    REQUIRE(vdo_open(v.data(), v.size(), fbc_bad.data(), fbc_bad.size()) == nullptr);
}

TEST_CASE("vdo decodes a keyframe and a per-pixel delta frame") {
    auto v = make_header(8, 8);  // 64 px, 8 groups

    // Frame 0: fill the 64-pixel canvas with palette index 7.
    auto f0 = frame_fill(64, 7);

    // Frame 1: mask path. Mask = group 0 fully copied (0xFF), rest kept (0x00);
    // raw source supplies the 8 new pixels = index 42.
    std::vector<uint8_t> f1;
    // mask sub-stream: count=8, dead u16, one literal of 8 bytes.
    std::vector<uint8_t> mask_stream(4, 0);
    put16(mask_stream, 0, 8);                 // output count = 8 groups
    mask_stream.push_back(8);                 // literal, 8 bytes
    mask_stream.push_back(0xFF);              // group 0: copy all 8
    for (int i = 0; i < 7; i++) mask_stream.push_back(0x00);
    uint16_t sz1 = (uint16_t)mask_stream.size();
    f1.resize(2);
    put16(f1, 0, (uint16_t)(0x8000 | sz1));   // tag: RLE mask, low15 = sz1
    f1.insert(f1.end(), mask_stream.begin(), mask_stream.end());
    // marker (non-zero, not 0xFFFF) -> raw source follows
    f1.push_back(0x01); f1.push_back(0x00);   // marker = 1
    for (int i = 0; i < 8; i++) f1.push_back(42);  // 8 raw source pixels

    v.insert(v.end(), f0.begin(), f0.end());
    v.insert(v.end(), f1.begin(), f1.end());
    auto fbc = make_fbc({ (uint32_t)f0.size(), (uint32_t)f1.size() });

    VdoDecoder* d = vdo_open(v.data(), v.size(), fbc.data(), fbc.size());
    REQUIRE(d != nullptr);
    REQUIRE(vdo_frame_count(d) == 2);

    auto frame0 = vdo_decode_frame(d, 0);
    REQUIRE(frame0.size() == 64);
    for (uint8_t px : frame0) REQUIRE(px == 7);

    auto frame1 = vdo_decode_frame(d, 1);
    REQUIRE(frame1.size() == 64);
    for (int i = 0; i < 8; i++) REQUIRE(frame1[i] == 42);   // group 0 changed
    for (int i = 8; i < 64; i++) REQUIRE(frame1[i] == 7);   // rest kept

    // Rewind: requesting frame 0 again replays cleanly.
    auto again = vdo_decode_frame(d, 0);
    for (uint8_t px : again) REQUIRE(px == 7);

    auto rgba = vdo_decode_frame_rgba(d, 1);
    REQUIRE(rgba.size() == 64 * 4);
    REQUIRE(rgba[3] == 255);  // opaque

    vdo_close(d);
}
