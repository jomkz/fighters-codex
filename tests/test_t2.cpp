#include <catch2/catch_test_macros.hpp>
#include <fx/t2.h>
#include <fx/ealib.h>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
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

// Reassemble a T2Map back into file bytes: raw header + both record arrays.
// The format has no other content, so this proves t2_read is lossless.
static std::vector<uint8_t> reassemble(const T2Map& map) {
    std::vector<uint8_t> out(map.header);
    auto append = [&](const std::vector<T2Record>& recs) {
        for (const auto& r : recs) {
            out.push_back(r.surface_class);
            out.push_back(r.elevation);
            out.push_back(r.texture_variant);
        }
    };
    append(map.leaves);
    append(map.summaries);
    return out;
}

TEST_CASE("t2_read parses header strings and grid dimensions") {
    auto buf = make_t2(2, 3);
    memcpy(buf.data() + 0x04, "Testland", 8);
    memcpy(buf.data() + 0x54, "tst.PIC", 7);
    T2Map map;
    REQUIRE(t2_read(buf.data(), buf.size(), &map));
    REQUIRE(map.theater   == "Testland");
    REQUIRE(map.atlas_pic == "tst.PIC");
    REQUIRE(map.tiles_w   == 2u);
    REQUIRE(map.tiles_h   == 3u);
    REQUIRE(map.leaf_step == 8u);
    REQUIRE(map.leaves_w  == 16u);
    REQUIRE(map.leaves_h  == 24u);
    REQUIRE(map.leaves.size()    == 16u * 24u);
    REQUIRE(map.summaries.size() == 2u * 3u);
}

TEST_CASE("t2_read decodes leaf and summary records row-major") {
    auto buf = make_t2(2, 2, 0xFF);
    const uint32_t leaves_w = 16;
    // Leaf (x=3, y=5): class 0xD2, band 7, variant 12
    size_t leaf_at = 0x95 + ((size_t)5 * leaves_w + 3) * 3;
    buf[leaf_at]     = 0xD2;
    buf[leaf_at + 1] = 7;
    buf[leaf_at + 2] = 12;
    // Summary (col=1, row=1): class 0xD0, band 2, variant 31
    size_t sum_off = 0x95 + (size_t)leaves_w * 16 * 3;
    size_t sum_at  = sum_off + ((size_t)1 * 2 + 1) * 3;
    buf[sum_at]     = 0xD0;
    buf[sum_at + 1] = 2;
    buf[sum_at + 2] = 31;

    T2Map map;
    REQUIRE(t2_read(buf.data(), buf.size(), &map));
    REQUIRE(map.leaf(3, 5).surface_class   == 0xD2);
    REQUIRE(map.leaf(3, 5).elevation       == 7);
    REQUIRE(map.leaf(3, 5).texture_variant == 12);
    REQUIRE(map.leaf(3, 4).surface_class   == 0x00); // neighbor untouched (make_t2 zero-fills leaves)
    REQUIRE(map.summary(1, 1).surface_class   == 0xD0);
    REQUIRE(map.summary(1, 1).elevation       == 2);
    REQUIRE(map.summary(1, 1).texture_variant == 31);
    REQUIRE(map.summary(0, 0).surface_class   == 0xFF);
}

TEST_CASE("t2_read is lossless: header + records reassemble byte-identical") {
    auto buf = make_t2(3, 2, 0xD2);
    // Non-trivial content in both arrays and the unknown header bytes
    buf[0x40] = 0xAB;
    for (size_t i = 0x95; i < buf.size(); i++) buf[i] = (uint8_t)(i * 31);
    T2Map map;
    REQUIRE(t2_read(buf.data(), buf.size(), &map));
    REQUIRE(map.header.size() == 0x95u);
    REQUIRE(reassemble(map) == buf);
}

TEST_CASE("t2_read rejects what t2_info rejects") {
    auto buf = make_t2(2, 2);
    T2Map map;
    buf[0] = 'X';
    REQUIRE_FALSE(t2_read(buf.data(), buf.size(), &map));
    buf[0] = 'B';
    buf.push_back(0);
    REQUIRE_FALSE(t2_read(buf.data(), buf.size(), &map));
}

TEST_CASE("arrays overlapping the header field map are rejected") {
    // Consistent extents, but the leaf array starts inside the header
    // (leaf_off 0x80 < 0x95) — the offset fields would overlap the payload.
    auto buf = make_t2(1, 1);
    auto wr32 = [&](size_t at, uint32_t v) {
        buf[at]     = (uint8_t)(v & 0xFF);
        buf[at + 1] = (uint8_t)((v >> 8) & 0xFF);
        buf[at + 2] = (uint8_t)((v >> 16) & 0xFF);
        buf[at + 3] = (uint8_t)((v >> 24) & 0xFF);
    };
    const uint32_t leaf_off    = 0x80;
    const uint32_t summary_off = leaf_off + 8 * 8 * 3;
    wr32(0x91, leaf_off);
    wr32(0x85, summary_off);
    buf.resize(summary_off + 3);
    T2Info info;
    T2Map  map;
    REQUIRE_FALSE(t2_info(buf.data(), buf.size(), &info));
    REQUIRE_FALSE(t2_read(buf.data(), buf.size(), &map));
}

TEST_CASE("t2_read decodes every stock theater losslessly") {
    const char* root = std::getenv("FX_FA_ROOT");
    if (!root || !*root)
        SKIP("FX_FA_ROOT not set (real-asset mode runs on the benches)");
    namespace fs = std::filesystem;
    auto lower = [](std::string s) {
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    // The 16 theater maps ship in FA_2.LIB (case varies across installs).
    std::vector<uint8_t> lib;
    for (const auto& de : fs::directory_iterator(root)) {
        if (!de.is_regular_file()) continue;
        if (lower(de.path().filename().string()) != "fa_2.lib") continue;
        std::ifstream f(de.path(), std::ios::binary | std::ios::ate);
        lib.resize((size_t)f.tellg());
        f.seekg(0);
        f.read((char*)lib.data(), (std::streamsize)lib.size());
        break;
    }
    if (lib.empty()) SKIP("fa_2.lib not present in this install");

    // Max leaf elevation band per theater (docs/fa/formats/T2.md § 3-Byte Record)
    const std::map<std::string, uint8_t> spec_max_band = {
        {"apa.t2", 12}, {"gre.t2", 4}, {"ira.t2", 6},  {"egy.t2", 6},
        {"nsk.t2", 12}, {"lfa.t2", 4}, {"ukr.t2", 6},  {"tviet.t2", 16},
    };

    auto entries = ealib_read_dir(lib.data(), lib.size());
    int total = 0;
    for (const auto& e : entries) {
        std::string name = lower(e.name);
        if (name.size() < 3 || name.substr(name.size() - 3) != ".t2") continue;
        auto data = ealib_extract(lib.data(), lib.size(), e, true);
        REQUIRE_FALSE(data.empty());

        INFO(name);
        T2Map map;
        REQUIRE(t2_read(data.data(), data.size(), &map));
        REQUIRE_FALSE(map.theater.empty());
        REQUIRE(lower(map.atlas_pic).size() > 4);
        REQUIRE(lower(map.atlas_pic).substr(map.atlas_pic.size() - 4) == ".pic");
        REQUIRE(map.leaf_step == 8u);

        // Spec claims: texture variants stay in 0..31 in both arrays
        uint8_t max_band = 0;
        for (const auto& r : map.leaves) {
            REQUIRE(r.texture_variant <= 31);
            if (r.elevation > max_band) max_band = r.elevation;
        }
        for (const auto& r : map.summaries)
            REQUIRE(r.texture_variant <= 31);

        auto it = spec_max_band.find(name);
        if (it != spec_max_band.end())
            REQUIRE(max_band == it->second);

        // Lossless: the decoded map reassembles byte-identical
        REQUIRE(reassemble(map) == data);
        ++total;
    }
    CHECK(total == 16);
    WARN("T2 read census: " << total << " theaters decoded losslessly");
}
