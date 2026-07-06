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

namespace {
fa::PolyVertex V(int x, int y) {
    fa::PolyVertex p;
    p.x = fa::ToFx(x);
    p.y = fa::ToFx(y);
    return p;
}
int CountIndex(const fa::Surface& s, std::uint8_t index) {
    int n = 0;
    for (int y = 0; y < s.height(); ++y) {
        const std::uint8_t* row = s.row(y);
        for (int x = 0; x < s.width(); ++x) {
            if (row[x] == index) ++n;
        }
    }
    return n;
}
}  // namespace

TEST_CASE("UPolygonToYLR matches hand-computed span endpoints", "[render][fa]") {
    // Right triangle (10,10)-(20,10)-(10,30): left edge vertical at x=10,
    // hypotenuse x(y) = 20 - (y-10)/2 (renderer.md s3.1 edge stepping).
    const fa::PolyVertex tri[3] = {V(10, 10), V(20, 10), V(10, 30)};
    fa::YlrList ylr;
    REQUIRE(fa::Raster::PolygonToYlr(tri, 3, ylr));
    REQUIRE(ylr.y_top == 10);
    REQUIRE(ylr.height() == 20);  // half-open [10, 30)
    for (int row = 0; row < 20; ++row) {
        CHECK(ylr.left[static_cast<std::size_t>(row)] == fa::ToFx(10));
    }
    CHECK(ylr.right[0] == fa::ToFx(20));                       // y=10 -> 20.0
    CHECK(ylr.right[1] == fa::ToFx(19) + fa::kFxOne / 2);      // y=11 -> 19.5
    CHECK(ylr.right[19] == fa::ToFx(10) + fa::kFxOne / 2);     // y=29 -> 10.5

    // Filled with truncated inclusive spans: widths 11,10,10,9,9,...,1 = 120.
    fa::Surface s(64, 64);
    s.Clear(0);
    fa::Raster r(s);
    r.SetColor(1);
    r.UPolygon(tri, 3);
    CHECK(CountIndex(s, 1) == 120);
    CHECK(s.row(10)[20] == 1);  // right endpoint inclusive on the top row
    CHECK(s.row(10)[21] == 0);
    CHECK(s.row(29)[10] == 1);  // last covered scanline
    CHECK(s.row(30)[10] == 0);  // y_max excluded (half-open vertical)
}

TEST_CASE("no_overlap makes horizontally abutting tiles seam-exact", "[render][fa]") {
    const fa::PolyVertex tile_a[4] = {V(0, 0), V(32, 0), V(32, 16), V(0, 16)};
    const fa::PolyVertex tile_b[4] = {V(32, 0), V(64, 0), V(64, 16), V(32, 16)};

    // Default (inclusive right): both tiles paint the shared column x=32.
    fa::Surface sa(64, 16), sb(64, 16);
    sa.Clear(0);
    sb.Clear(0);
    fa::Raster ra(sa), rb(sb);
    ra.SetColor(1);
    ra.UPolygon(tile_a, 4);
    rb.SetColor(2);
    rb.UPolygon(tile_b, 4);
    CHECK(sa.row(5)[32] == 1);
    CHECK(sb.row(5)[32] == 2);  // overdraw at the seam without the flag

    // _no_overlap: every pixel of the row is painted exactly once, no gap.
    fa::Surface s(64, 16);
    s.Clear(0);
    fa::Raster r(s);
    r.SetNoOverlap(true);
    r.SetColor(1);
    r.UPolygon(tile_a, 4);
    r.SetColor(2);
    r.UPolygon(tile_b, 4);
    CHECK(CountIndex(s, 1) == 32 * 16);
    CHECK(CountIndex(s, 2) == 32 * 16);
    CHECK(CountIndex(s, 0) == 0);
    CHECK(s.row(5)[31] == 1);
    CHECK(s.row(5)[32] == 2);
}

TEST_CASE("vertically abutting tiles never overdraw (half-open scanlines)", "[render][fa]") {
    const fa::PolyVertex top[4] = {V(0, 0), V(16, 0), V(16, 16), V(0, 16)};
    const fa::PolyVertex bottom[4] = {V(0, 16), V(16, 16), V(16, 32), V(0, 32)};
    fa::Surface s(16, 32);
    s.Clear(0);
    fa::Raster r(s);
    r.SetNoOverlap(true);
    r.SetColor(1);
    r.UPolygon(top, 4);
    r.SetColor(2);
    r.UPolygon(bottom, 4);
    CHECK(CountIndex(s, 1) == 16 * 16);
    CHECK(CountIndex(s, 2) == 16 * 16);
    CHECK(s.row(15)[8] == 1);
    CHECK(s.row(16)[8] == 2);
}

TEST_CASE("degenerate polygons and fully clipped spans are no-ops", "[render][fa]") {
    fa::YlrList ylr;
    const fa::PolyVertex two[2] = {V(0, 0), V(10, 10)};
    CHECK_FALSE(fa::Raster::PolygonToYlr(two, 2, ylr));

    // A sub-scanline sliver (y from ~10.2 to ~10.8) covers no scanline.
    fa::PolyVertex sliver[3] = {V(0, 0), V(10, 0), V(5, 0)};
    sliver[0].y = fa::ToFx(10) + 13107;   // ~10.2
    sliver[1].y = fa::ToFx(10) + 13107;
    sliver[2].y = fa::ToFx(10) + 52429;   // ~10.8
    CHECK_FALSE(fa::Raster::PolygonToYlr(sliver, 3, ylr));

    // A polygon entirely right of the clip box paints nothing.
    fa::Surface s(64, 64);
    s.Clear(0);
    fa::Raster r(s);
    r.SetColor(1);
    const fa::PolyVertex off[4] = {V(100, 0), V(200, 0), V(200, 50), V(100, 50)};
    r.UPolygon(off, 4);
    CHECK(CountIndex(s, 1) == 0);
}

TEST_CASE("Gouraud spans interpolate the packed index; endpoints exact", "[render][fa]") {
    // Horizontal shade ramp: c from 10 at x=0 to 74 at x=64 -> pixel x gets
    // index 10+x, exactly (renderer.md s3.1: c_packed rides the same 16.16
    // stepping as x). The ramp is an index ramp, not an RGB blend.
    fa::PolyVertex quad[4] = {V(0, 0), V(64, 0), V(64, 8), V(0, 8)};
    quad[0].c = fa::ToFx(10);
    quad[3].c = fa::ToFx(10);
    quad[1].c = fa::ToFx(74);
    quad[2].c = fa::ToFx(74);

    fa::Surface s(64, 8);
    s.Clear(0);
    fa::Raster r(s);
    r.SUPolygon(quad, 4);
    for (int x = 0; x < 64; ++x) {
        CHECK(s.row(3)[x] == 10 + x);  // monotonic, endpoint-exact
    }

    // Vertical ramp: c follows the edges scanline by scanline.
    fa::PolyVertex vquad[4] = {V(0, 0), V(16, 0), V(16, 16), V(0, 16)};
    vquad[2].c = fa::ToFx(16);
    vquad[3].c = fa::ToFx(16);
    fa::Surface sv(16, 16);
    sv.Clear(255);
    fa::Raster rv(sv);
    rv.SUPolygon(vquad, 4);
    for (int y = 0; y < 16; ++y) {
        CHECK(sv.row(y)[8] == y);
    }
}

TEST_CASE("fill-type dispatch selects flat vs Gouraud like sh_op_80 stages it", "[render][fa]") {
    // sh_op_80 (render-core.md): gouraudOn == 0 -> SetFlatColor; otherwise
    // the shaded path. Here: FillType::Flat ignores per-vertex c and paints
    // _cColor; FillType::Shaded interpolates c through the same Polygon call.
    fa::PolyVertex quad[4] = {V(0, 0), V(32, 0), V(32, 8), V(0, 8)};
    quad[1].c = fa::ToFx(200);
    quad[2].c = fa::ToFx(200);

    fa::Surface s(32, 8);
    s.Clear(0);
    fa::Raster r(s);
    r.SetColor(7);
    r.SetFillType(fa::FillType::Flat);
    r.Polygon(quad, 4);
    CHECK(s.row(4)[1] == 7);
    CHECK(s.row(4)[30] == 7);  // flat: c ignored

    r.SetFillType(fa::FillType::Shaded);
    r.Polygon(quad, 4);
    CHECK(s.row(4)[0] == 0);    // shaded: left endpoint c=0
    CHECK(s.row(4)[16] == 100); // midpoint of the 0..200 ramp over 32 px
    CHECK(s.row(4)[31] > 190);  // approaching the right endpoint
}

TEST_CASE("Gouraud c clamps to the palette range", "[render][fa]") {
    // Out-of-range c (inferred: the engine never emits it) must not wrap.
    fa::PolyVertex quad[4] = {V(0, 0), V(8, 0), V(8, 4), V(0, 4)};
    for (auto& p : quad) p.c = fa::ToFx(500);
    fa::Surface s(8, 4);
    s.Clear(1);
    fa::Raster r(s);
    r.SUPolygon(quad, 4);
    CHECK(s.row(2)[4] == 255);
}

TEST_CASE("fa spans stay correct past the 1024 ceiling", "[render][fa]") {
    // Resolution-independence acceptance (#329): the span core at 2560x1440.
    fa::Surface s(2560, 1440);
    s.Clear(0);
    fa::Raster r(s);
    r.SetColor(6);
    const fa::PolyVertex quad[4] = {V(1900, 700), V(2400, 700), V(2400, 900), V(1900, 900)};
    r.UPolygon(quad, 4);
    CHECK(CountIndex(s, 6) == 501 * 200);  // inclusive right edge, half-open rows
    CHECK(s.row(800)[2300] == 6);
    CHECK(s.row(800)[1899] == 0);
    CHECK(s.row(800)[2401] == 0);
}
