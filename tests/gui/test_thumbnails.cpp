#include <catch2/catch_test_macros.hpp>
#include "asset_index.h"
#include "editors/sh_scene.h"
#include "thumbnails.h"
#include "workspace.h"
#include <fx/ealib.h>
#include <fx/pal.h>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using namespace fxg;

// ---------------------------------------------------------------------------
// Helpers — a minimal parseable SH (the same synthetic Phar Lap wrapper the
// fx_tests SH suite uses): one 3-vertex buffer, one coloured face, EndShape.
// ---------------------------------------------------------------------------

static std::vector<uint8_t> wrap_sh(const std::vector<uint8_t>& code) {
    const uint32_t PE_OFF   = 64;
    const uint32_t SEC_OFF  = PE_OFF + 24;
    const uint32_t CODE_OFF = SEC_OFF + 40;
    std::vector<uint8_t> buf(CODE_OFF + code.size(), 0);
    buf[0] = 'M'; buf[1] = 'Z';
    buf[0x3C] = (uint8_t)PE_OFF;
    buf[PE_OFF + 0] = 'P'; buf[PE_OFF + 1] = 'L';
    buf[PE_OFF + 4] = 0x4C; buf[PE_OFF + 5] = 0x01;
    buf[PE_OFF + 6] = 1;
    std::memcpy(buf.data() + SEC_OFF, "CODE", 4);
    buf[SEC_OFF + 8]  = (uint8_t)code.size();
    buf[SEC_OFF + 12] = 0x80;
    buf[SEC_OFF + 16] = (uint8_t)code.size();
    buf[SEC_OFF + 20] = (uint8_t)(CODE_OFF & 0xFF);
    buf[SEC_OFF + 21] = (uint8_t)((CODE_OFF >> 8) & 0xFF);
    std::memcpy(buf.data() + CODE_OFF, code.data(), code.size());
    return buf;
}

static std::vector<uint8_t> make_min_sh() {
    std::vector<uint8_t> c(14, 0);
    c[0] = 0xFF; c[1] = 0xFF; c[6] = 8;                    // header, scale 8
    c[8] = 10; c[10] = 10; c[12] = 10;
    std::vector<uint8_t> vb = {0x82, 0x00, 3, 0, 0, 0,
        10,0, 0,0, 0,0,  (uint8_t)0xF6,(uint8_t)0xFF, 0,0, 0,0,  0,0, 10,0, 0,0};
    c.insert(c.end(), vb.begin(), vb.end());
    c.insert(c.end(), {0xFC, 0x00, 0x00, 7, 0x00, 3, 0, 1, 2});  // colour 7
    c.push_back(0x01);                                            // EndShape
    return wrap_sh(c);
}

// A palette where index 7 is pure red, so the rendered face is unmistakable.
static fx::Palette red7_palette() {
    fx::Palette pal{};
    pal.r[7] = 255;
    return pal;
}

static int red_pixels(const fx_render::Image& img) {
    int n = 0;
    for (int y = 0; y < img.height; ++y)
        for (int x = 0; x < img.width; ++x) {
            const uint8_t* p = img.at(x, y);
            if (p[0] > 100 && p[1] < 50 && p[2] < 50) n++;
        }
    return n;
}

// Drain until `want` results arrive (or a timeout) — the worker is async.
static std::vector<ThumbnailService::Result>
drain_n(ThumbnailService& svc, size_t want) {
    std::vector<ThumbnailService::Result> got;
    for (int i = 0; i < 500 && got.size() < want; ++i) {
        for (auto& r : svc.Drain()) got.push_back(std::move(r));
        if (got.size() < want)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return got;
}

// ---------------------------------------------------------------------------
// The display-free render core
// ---------------------------------------------------------------------------

TEST_CASE("RenderShThumbnail rasterises a shape headless", "[gui][thumbnails]") {
    auto sh = make_min_sh();
    fx_render::Image img =
        RenderShThumbnail(sh.data(), sh.size(), red7_palette(), nullptr, 64);
    REQUIRE(img.width == 64);
    REQUIRE(img.height == 64);
    CHECK(red_pixels(img) > 0);  // the colour-7 face landed on screen

    // Garbage records yield the empty image, never a crash.
    CHECK(RenderShThumbnail(sh.data(), 8, red7_palette(), nullptr, 64).width == 0);
}

// ---------------------------------------------------------------------------
// The async service: graph resolution, negative results, and the disk cache
// ---------------------------------------------------------------------------

TEST_CASE("ThumbnailService renders through the graph and serves the disk cache",
          "[gui][thumbnails]") {
    // TEST.PT names FOO.SH; NOWHERE.JT names no shape at all.
    const char* pt =
        "[brent's_relocatable_format]\r\n"
        ":shape\r\n"
        "\tstring \"FOO.SH\"\r\n"
        "\tend\r\n";
    const char* jt =
        "[brent's_relocatable_format]\r\n"
        ":shape\r\n"
        "\tend\r\n";
    // The install palette the service resolves — raw 6-bit VGA with index 7
    // pure red, so the rendered face is checkable.
    std::vector<uint8_t> palBytes(768, 0);
    fx::pal_save(red7_palette(), palBytes.data());
    std::vector<std::pair<std::string, std::vector<uint8_t>>> files = {
        { "TEST.PT",     std::vector<uint8_t>(pt, pt + std::strlen(pt)) },
        { "FOO.SH",      make_min_sh() },
        { "NOWHERE.JT",  std::vector<uint8_t>(jt, jt + std::strlen(jt)) },
        { "PALETTE.PAL", palBytes },
    };
    fs::path root  = fs::temp_directory_path() / "fxs_thumbs_test";
    fs::path cache = root / "cache";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root);
    {
        auto lib = fx::ealib_build(files);
        std::ofstream f(root / "TEST.LIB", std::ios::binary);
        f.write((const char*)lib.data(), (std::streamsize)lib.size());
    }

    Workspace ws = workspace_scan(root.string());
    AssetIndex idx = asset_index_build(ws);
    REQUIRE(idx.built);
    auto node_of = [&](const char* n) {
        const WorkspaceEntry* e = ws.find(n);
        REQUIRE(e != nullptr);
        return (int)(e - ws.names.data());
    };
    const int pt_n = node_of("TEST.PT");
    const int jt_n = node_of("NOWHERE.JT");

    // First run: the PT resolves its shape through the graph and renders; the
    // shapeless JT reports a negative result.
    {
        ThumbnailService svc;
        svc.Start(ws, idx, cache.string(), 64);
        svc.Request(pt_n);
        svc.Request(jt_n);
        svc.Request(pt_n);  // duplicate requests collapse
        auto got = drain_n(svc, 2);
        REQUIRE(got.size() == 2);
        for (auto& r : got) {
            if (r.node == pt_n) {
                CHECK(r.image.width == 64);
                CHECK(red_pixels(r.image) > 0);
            } else {
                CHECK(r.image.width == 0);
            }
        }
        CHECK(svc.renders() == 1);
        CHECK(svc.diskHits() == 0);
        CHECK(svc.pending() == 0);
    }

    // Second run over the same cache dir: the render is served from disk —
    // the "second open renders no thumbnails" acceptance path.
    {
        ThumbnailService svc;
        svc.Start(ws, idx, cache.string(), 64);
        svc.Request(pt_n);
        auto got = drain_n(svc, 1);
        REQUIRE(got.size() == 1);
        CHECK(got[0].image.width == 64);
        CHECK(red_pixels(got[0].image) > 0);
        CHECK(svc.renders() == 0);
        CHECK(svc.diskHits() == 1);
    }

    fs::remove_all(root, ec);
}

// ---------------------------------------------------------------------------
// Real-install spot check (FX_FA_ROOT): the A10 renders a thumbnail
// ---------------------------------------------------------------------------

TEST_CASE("ThumbnailService renders the A10 on a real install",
          "[gui][thumbnails][fa-root]") {
    const char* root = std::getenv("FX_FA_ROOT");
    if (!root || !*root) {
        SKIP("FX_FA_ROOT not set (real-asset mode runs on the benches)");
    }

    Workspace ws = workspace_scan(root);
    REQUIRE(ws.mounted());
    AssetIndex idx = asset_index_build(ws);
    REQUIRE(idx.built);
    const WorkspaceEntry* e = ws.find("A10.PT");
    REQUIRE(e != nullptr);

    ThumbnailService svc;
    svc.Start(ws, idx, "", 128);  // memory-only
    svc.Request((int)(e - ws.names.data()));
    auto got = drain_n(svc, 1);
    REQUIRE(got.size() == 1);
    REQUIRE(got[0].image.width == 128);

    // The image is not just the cleared backdrop.
    int lit = 0;
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x) {
            const uint8_t* p = got[0].image.at(x, y);
            if (p[0] + p[1] + p[2] > 120) lit++;
        }
    CHECK(lit > 100);
    CHECK(svc.renders() == 1);
}
