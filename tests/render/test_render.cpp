// fx_render software backend — headless rasteriser checks.
#include "fx_render/render.h"

#include <catch2/catch_test_macros.hpp>

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
