// fx_render software backend — headless rasteriser checks.
#include "fx_render/render.h"

#include <catch2/catch_test_macros.hpp>
#include <cstdlib>

using namespace fx_render;

TEST_CASE("Image::resize allocates RGBA storage", "[render]") {
    Image img;
    img.resize(4, 3);
    REQUIRE(img.width == 4);
    REQUIRE(img.height == 3);
    REQUIRE(img.pixels.size() == 4u * 3u * 4u);
}

TEST_CASE("MakeRenderer: software available, OpenGL via gl.h", "[render]") {
    REQUIRE(MakeRenderer(Backend::Software) != nullptr);
    REQUIRE(MakeRenderer(Backend::OpenGL) == nullptr);  // constructed via fx_render/gl.h
}

TEST_CASE("Software backend fills a triangle and clears the rest", "[render]") {
    auto r = MakeRenderer(Backend::Software);
    REQUIRE(r);
    REQUIRE(r->backend() == Backend::Software);

    // A red triangle around NDC origin; identity MVP so NDC == object space.
    Mesh mesh;
    mesh.vertices = {
        {-0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f},
        { 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f},
        { 0.0f,  0.5f, 0.0f, 1.0f, 0.0f, 0.0f},
    };
    Image img;
    img.resize(64, 64);
    RenderToImage(*r, mesh, Camera{}, img, {0, 0, 0, 255}, {});

    // Centre pixel (NDC 0,0) is inside the triangle -> red.
    const std::uint8_t* c = img.at(32, 32);
    CHECK(c[0] > 200);
    CHECK(c[1] < 50);
    CHECK(c[2] < 50);
    CHECK(c[3] == 255);

    // A top-left corner pixel is outside -> untouched clear colour.
    const std::uint8_t* corner = img.at(1, 1);
    CHECK(corner[0] == 0);
    CHECK(corner[1] == 0);
    CHECK(corner[2] == 0);
}

TEST_CASE("Software backend samples the mesh texture instead of vertex colour", "[render]") {
    auto r = MakeRenderer(Backend::Software);

    // 2x2 texture: texel (0,0) is red; the rest distinct.
    auto tex = std::make_shared<Image>();
    tex->resize(2, 2);
    auto set = [&](int x, int y, std::uint8_t rr, std::uint8_t gg, std::uint8_t bb) {
        std::uint8_t* p = tex->at(x, y);
        p[0] = rr; p[1] = gg; p[2] = bb; p[3] = 255;
    };
    set(0, 0, 255, 0, 0);  set(1, 0, 0, 255, 0);
    set(0, 1, 0, 0, 255);  set(1, 1, 255, 255, 255);

    // Vertex colour is GREEN, but every corner's UV points at texel (0,0)=red,
    // so a textured draw must produce red — proving texture overrides colour.
    Mesh mesh;
    mesh.texture = tex;
    mesh.vertices = {
        {-0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.1f, 0.1f},
        { 0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.1f, 0.1f},
        { 0.0f,  0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.1f, 0.1f},
    };
    Image img;
    img.resize(64, 64);
    RenderToImage(*r, mesh, Camera{}, img, {0, 0, 0, 255}, {});

    const std::uint8_t* c = img.at(32, 32);
    CHECK(c[0] > 200);  // red from the texture
    CHECK(c[1] < 50);   // not the green vertex colour
    CHECK(c[2] < 50);
}

TEST_CASE("Software backend respects the clear colour", "[render]") {
    auto r = MakeRenderer(Backend::Software);
    Image img;
    img.resize(8, 8);
    RenderToImage(*r, Mesh{}, Camera{}, img, {10, 20, 30, 255}, {});  // empty mesh -> just clear
    const std::uint8_t* p = img.at(4, 4);
    CHECK(p[0] == 10);
    CHECK(p[1] == 20);
    CHECK(p[2] == 30);
}

TEST_CASE("Software backend draws a line list", "[render]") {
    auto r = MakeRenderer(Backend::Software);
    Mesh line;
    line.vertices = {{-0.9f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f},
                     { 0.9f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f}};  // horizontal, through NDC y=0
    Image img;
    img.resize(32, 32);
    DrawOptions opts;
    opts.primitive = Primitive::Lines;
    RenderToImage(*r, line, Camera{}, img, {0, 0, 0, 255}, opts);
    // The line crosses the middle row -> the centre pixel is white.
    const std::uint8_t* c = img.at(16, 16);
    CHECK(c[0] > 200);
    CHECK(c[1] > 200);
    CHECK(c[2] > 200);
}

TEST_CASE("Software backend is resolution-independent above FA's 1024x768 ceiling", "[render]") {
    // FA.EXE's 1024x768 maximum is a GG_/DirectDraw *mode* limit, not a G_
    // algorithm property; fx_render targets must stay dimension-parametric
    // (#290/#328). Render one NDC scene at the 4:3 reference size and at a
    // 16:9 size past the ceiling: coverage and colour must agree at
    // corresponding normalized sample points.
    auto r = MakeRenderer(Backend::Software);
    REQUIRE(r);

    Mesh mesh;
    mesh.vertices = {
        {-0.8f, -0.8f, 0.0f, 1.0f, 0.0f, 0.0f},
        { 0.8f, -0.8f, 0.0f, 0.0f, 1.0f, 0.0f},
        { 0.0f,  0.9f, 0.0f, 0.0f, 0.0f, 1.0f},
    };

    auto render_at = [&](int w, int h) {
        Image img;
        img.resize(w, h);
        RenderToImage(*r, mesh, Camera{}, img, {0, 0, 0, 255}, {});
        return img;
    };
    const Image ref = render_at(640, 480);    // FA's 4:3 reference mode
    const Image big = render_at(2560, 1440);  // past the DirectDraw mode list

    // Normalized (u, v) sample points, top-left origin. The point near the
    // bottom-right vertex lands past x = 1024 in the big image.
    struct Sample { float u, v; bool inside; };
    const Sample samples[] = {
        {0.50f, 0.60f, true},   // around the centroid
        {0.15f, 0.85f, true},   // near the bottom-left vertex
        {0.85f, 0.85f, true},   // near the bottom-right vertex (x ~ 2176)
        {0.50f, 0.15f, true},   // near the top vertex
        {0.02f, 0.02f, false},  // outside: corners, mid-left, below the base
        {0.98f, 0.02f, false},
        {0.02f, 0.50f, false},
        {0.50f, 0.98f, false},
    };
    auto at = [](const Image& img, float u, float v) {
        return img.at(static_cast<int>(u * img.width), static_cast<int>(v * img.height));
    };
    for (const Sample& s : samples) {
        const std::uint8_t* a = at(ref, s.u, s.v);
        const std::uint8_t* b = at(big, s.u, s.v);
        if (s.inside) {
            // Gouraud-interpolated colour: identical up to the sub-pixel
            // offset between the two grids' sample centres.
            for (int ch = 0; ch < 3; ++ch) {
                CHECK(std::abs(static_cast<int>(a[ch]) - static_cast<int>(b[ch])) <= 4);
            }
            CHECK(a[3] == 255);
            CHECK(b[3] == 255);
        } else {
            for (int ch = 0; ch < 3; ++ch) {  // untouched clear colour in both
                CHECK(a[ch] == 0);
                CHECK(b[ch] == 0);
            }
        }
    }
}
