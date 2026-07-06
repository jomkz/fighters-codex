#include <catch2/catch_test_macros.hpp>
#include "editors/terrain_preview.h"
#include "fx/t2.h"
#include "fx_render/render.h"
#include <cstring>
#include <memory>
#include <vector>

using namespace fxg;

// Minimal synthetic T2 in the engine layout (mirrors tests/test_t2.cpp's
// make_t2): 0x95 header, leaf array (leaves_w*leaves_h x 3B), summary array.
static std::vector<uint8_t> make_t2(uint32_t tiles_w, uint32_t tiles_h,
                                    uint8_t surface_class, uint8_t variant,
                                    uint8_t elevation, uint32_t leaf_step = 8) {
    const uint32_t HEADER = 0x95;
    uint32_t leaves_w = tiles_w * leaf_step, leaves_h = tiles_h * leaf_step;
    uint32_t leaf_off = HEADER;
    uint32_t summary_off = leaf_off + leaves_w * leaves_h * 3;
    size_t total = summary_off + tiles_w * tiles_h * 3;
    std::vector<uint8_t> buf(total, 0);
    std::memcpy(buf.data(), "BIT2", 4);
    auto wr32 = [&](size_t at, uint32_t v) {
        buf[at] = v & 0xFF; buf[at+1] = (v>>8)&0xFF;
        buf[at+2] = (v>>16)&0xFF; buf[at+3] = (v>>24)&0xFF;
    };
    wr32(0x79, leaf_step); wr32(0x7D, tiles_w); wr32(0x81, tiles_h);
    wr32(0x85, summary_off); wr32(0x89, leaves_w); wr32(0x8D, leaves_h);
    wr32(0x91, leaf_off);
    // Fill leaves with the given class/elevation/variant.
    for (uint32_t i = 0; i < leaves_w * leaves_h; i++) {
        buf[leaf_off + i*3 + 0] = surface_class;
        buf[leaf_off + i*3 + 1] = elevation;
        buf[leaf_off + i*3 + 2] = variant;
    }
    for (uint32_t t = 0; t < tiles_w * tiles_h; t++)
        buf[summary_off + t*3] = surface_class;
    return buf;
}

static std::shared_ptr<fx_render::Image> solid_tile(int n) {
    auto img = std::make_shared<fx_render::Image>();
    img->resize(n, n);
    for (auto& p : img->pixels) p = 200;
    return img;
}

TEST_CASE("BuildTerrainScene: all-land grid triangulates into per-variant meshes") {
    auto buf = make_t2(2, 2, /*class*/0xD2, /*variant*/3, /*elev*/4);
    fx::T2Map map;
    REQUIRE(fx::t2_read(buf.data(), buf.size(), &map));

    auto tile = solid_tile(8);
    TerrainScene scene = BuildTerrainScene(
        map, [&](std::uint8_t v) -> std::shared_ptr<const fx_render::Image> {
            return v == 3 ? tile : nullptr;
        });

    // 16x16 leaves, all land, all variant 3 -> one textured mesh, no water.
    REQUIRE(scene.water_leaves == 0);
    REQUIRE(scene.land_leaves == 16 * 16);
    REQUIRE(scene.textured_leaves == 16 * 16);
    REQUIRE(scene.textured.size() == 1u);
    REQUIRE(scene.textured[0].texture == tile);
    // Two triangles (6 verts) per leaf.
    REQUIRE(scene.textured[0].vertices.size() == (size_t)16 * 16 * 6);
    REQUIRE(scene.flat.vertices.empty());
    REQUIRE(scene.span > 0.0f);
}

TEST_CASE("BuildTerrainScene: water goes to the flat mesh, not textured") {
    auto buf = make_t2(1, 1, /*class*/0xFF, /*variant*/0, /*elev*/1);
    fx::T2Map map;
    REQUIRE(fx::t2_read(buf.data(), buf.size(), &map));

    TerrainScene scene = BuildTerrainScene(
        map, [](std::uint8_t) -> std::shared_ptr<const fx_render::Image> {
            return nullptr;  // even if a tile existed, water isn't textured
        });
    REQUIRE(scene.water_leaves == 8 * 8);
    REQUIRE(scene.textured.empty());
    REQUIRE(scene.flat.vertices.size() == (size_t)8 * 8 * 6);
}

TEST_CASE("BuildTerrainScene: land with no available tile falls back to flat") {
    auto buf = make_t2(1, 1, 0xD2, 5, 2);
    fx::T2Map map;
    REQUIRE(fx::t2_read(buf.data(), buf.size(), &map));
    TerrainScene scene = BuildTerrainScene(
        map, [](std::uint8_t) -> std::shared_ptr<const fx_render::Image> {
            return nullptr;
        });
    REQUIRE(scene.land_leaves == 8 * 8);
    REQUIRE(scene.textured_leaves == 0);
    REQUIRE(scene.textured.empty());
    REQUIRE(scene.flat.vertices.size() == (size_t)8 * 8 * 6);
}

TEST_CASE("BuildTerrainScene renders to a non-empty software image") {
    auto buf = make_t2(2, 2, 0xD2, 1, 6);
    fx::T2Map map;
    REQUIRE(fx::t2_read(buf.data(), buf.size(), &map));
    auto tile = solid_tile(8);
    TerrainScene scene = BuildTerrainScene(
        map, [&](std::uint8_t) { return tile; });
    REQUIRE_FALSE(scene.textured.empty());

    auto r = fx_render::MakeRenderer(fx_render::Backend::Software);
    auto rt = r->MakeTarget(64, 64);
    // Top-down-ish identity-ish camera: just prove the pipeline runs and the
    // target reads back the cleared+drawn surface without crashing.
    fx_render::Camera cam;  // identity MVP
    r->Begin(*rt, {0, 0, 0, 255});
    fx_render::DrawOptions o; o.primitive = fx_render::Primitive::Triangles;
    for (auto& m : scene.textured) r->Draw(m, cam, o);
    r->End();
    fx_render::Image img; rt->Read(img);
    REQUIRE(img.width == 64);
    REQUIRE(img.height == 64);
    REQUIRE(img.pixels.size() == (size_t)64 * 64 * 4);
}

TEST_CASE("FillDefaultTerrainBand only touches indices 192..255") {
    fx::Palette pal{};
    for (int i = 0; i < 256; i++) { pal.r[i] = 7; pal.g[i] = 7; pal.b[i] = 7; }
    FillDefaultTerrainBand(pal);
    for (int i = 0; i < 192; i++) {
        REQUIRE(pal.r[i] == 7); REQUIRE(pal.g[i] == 7); REQUIRE(pal.b[i] == 7);
    }
    // The band is filled (not left at the sentinel 7 everywhere).
    bool any_changed = false;
    for (int i = 192; i < 256; i++)
        if (pal.r[i] != 7 || pal.g[i] != 7 || pal.b[i] != 7) any_changed = true;
    REQUIRE(any_changed);
}
