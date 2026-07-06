// fx_render::fa — FA-faithful surface / palette / raster-state checks (#328).
#include "fx_render/fa.h"

#include <catch2/catch_test_macros.hpp>

#include "fx_render/render.h"

using namespace fx_render;

TEST_CASE("fa::Surface carries runtime dimensions and a row-pointer table", "[render][fa]") {
    // Dimensions are constructor parameters, per the documented bitmap record
    // (renderer.md s7: width/height/stride + row pointers) — no mode table.
    fa::Surface s(640, 480, 1024);  // stride wider than the visible row
    REQUIRE(s.width() == 640);
    REQUIRE(s.height() == 480);
    REQUIRE(s.stride() == 1024);
    // Row pointers advance by exactly the stride, like the _cb +0x22 table.
    for (int y = 1; y < s.height(); ++y) {
        CHECK(s.row(y) - s.row(y - 1) == s.stride());
    }

    s.Clear(7);
    CHECK(s.row(0)[0] == 7);
    CHECK(s.row(479)[639] == 7);

    // Degenerate and odd sizes stay well-formed.
    fa::Surface odd(1023, 769);
    CHECK(odd.stride() == 1023);
    fa::Surface empty(0, 0);
    CHECK(empty.width() == 0);
}

TEST_CASE("fa::Surface stays parametric above FA's 1024x768 ceiling", "[render][fa]") {
    // The ceiling is a GG_/DirectDraw device limit (renderer.md s7); the fa
    // surface must scale past it (#340 guard, extended to the fa path).
    fa::Surface s(2560, 1440);
    s.Clear(3);
    fa::Raster r(s);
    REQUIRE(r.clip_right() == 2559);
    REQUIRE(r.clip_bottom() == 1439);
    r.SetColor(9);
    r.Point(2000, 1200);  // far past any FA.EXE mode in both axes
    CHECK(s.row(1200)[2000] == 9);
    CHECK(s.row(1200)[1999] == 3);
}

TEST_CASE("fa::Palette presents indexed pixels as RGBA with the VGA 6-bit expansion", "[render][fa]") {
    fa::Surface s(3, 2, 8);  // stride != width: presentation must honour it
    s.Clear(0);
    s.row(0)[1] = 5;
    s.row(1)[2] = 200;  // past the 192-entry palette -> presents as black

    fa::Palette pal;
    pal.entries[5] = {63, 0, 32};  // 6-bit components

    Image img;
    pal.Present(s, img);
    REQUIRE(img.width == 3);
    REQUIRE(img.height == 2);

    const std::uint8_t* p = img.at(1, 0);
    CHECK(p[0] == 255);  // 63 -> 255
    CHECK(p[1] == 0);
    CHECK(p[2] == 130);  // 32 -> (32<<2)|(32>>4) = 130
    CHECK(p[3] == 255);

    const std::uint8_t* zero = img.at(0, 0);   // palette entry 0 is black
    CHECK(zero[0] == 0);
    CHECK(zero[1] == 0);
    CHECK(zero[2] == 0);
    const std::uint8_t* oob = img.at(2, 1);    // out-of-palette index
    CHECK(oob[0] == 0);
    CHECK(oob[3] == 255);
}

TEST_CASE("fa::Palette nearest-index remap picks the closest entry", "[render][fa]") {
    // The G_RemapBitmapToPalette operation: nearest colour by distance.
    fa::Palette pal;
    pal.entries[1] = {63, 0, 0};    // red
    pal.entries[2] = {0, 63, 0};    // green
    pal.entries[3] = {32, 32, 32};  // mid grey
    CHECK(pal.Nearest(255, 0, 0) == 1);
    CHECK(pal.Nearest(0, 255, 0) == 2);
    CHECK(pal.Nearest(120, 130, 125) == 3);
    CHECK(pal.Nearest(0, 0, 0) == 0);  // exact hit on default black
}

TEST_CASE("fa::Raster clip box opens full-surface and clamps like G_SetClipBox", "[render][fa]") {
    fa::Surface s(320, 200);
    fa::Raster r(s);
    // G_Init: full surface.
    CHECK(r.clip_left() == 0);
    CHECK(r.clip_top() == 0);
    CHECK(r.clip_right() == 319);
    CHECK(r.clip_bottom() == 199);

    r.SetClipBox(-10, -10, 5000, 5000);  // clamped to the surface
    CHECK(r.clip_left() == 0);
    CHECK(r.clip_right() == 319);
    CHECK(r.clip_bottom() == 199);

    r.SetClipBox(10, 20, 100, 90);
    CHECK(r.clip_left() == 10);
    CHECK(r.clip_top() == 20);
    CHECK(r.clip_right() == 100);
    CHECK(r.clip_bottom() == 90);

    // State block: _cColor / _cFillType round-trip.
    r.SetColor(42);
    CHECK(r.color() == 42);
    r.SetFillType(fa::FillType::Shaded);
    CHECK(r.fill_type() == fa::FillType::Shaded);
}

TEST_CASE("fa::Raster point and rect honour the clip box", "[render][fa]") {
    fa::Surface s(64, 64);
    s.Clear(0);
    fa::Raster r(s);
    r.SetClipBox(8, 8, 15, 15);  // inclusive bounds
    r.SetColor(1);

    r.Point(8, 8);    // corners of the clip box: in
    r.Point(15, 15);
    r.Point(7, 8);    // one step outside each edge: out
    r.Point(8, 7);
    r.Point(16, 15);
    r.Point(15, 16);
    CHECK(s.row(8)[8] == 1);
    CHECK(s.row(15)[15] == 1);
    CHECK(s.row(8)[7] == 0);
    CHECK(s.row(7)[8] == 0);
    CHECK(s.row(15)[16] == 0);
    CHECK(s.row(16)[15] == 0);

    // A rect straddling the whole box paints exactly the 8x8 clip region.
    r.SetColor(2);
    r.Rect(0, 0, 63, 63);
    int painted = 0;
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            if (s.row(y)[x] == 2) ++painted;
        }
    }
    CHECK(painted == 8 * 8);
    CHECK(s.row(8)[8] == 2);
    CHECK(s.row(15)[15] == 2);
}

TEST_CASE("fa fixed-point helpers round-trip pixel coordinates", "[render][fa]") {
    // 16.16 per renderer.md s3.1; integer range covers well past 8K screens.
    CHECK(fa::ToFx(0) == 0);
    CHECK(fa::ToFx(1) == fa::kFxOne);
    CHECK(fa::FxFloor(fa::ToFx(2560)) == 2560);
    CHECK(fa::FxFloor(fa::ToFx(7680) + fa::kFxOne / 2) == 7680);
    CHECK(fa::FxFloor(-fa::kFxOne) == -1);
}
