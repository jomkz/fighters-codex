// fx_render — a shared, backend-agnostic renderer for the FA reconstruction.
//
// One geometry->pixels API with interchangeable backends, so the OpenGL and
// software render paths are built once and shared by fxs (the asset viewer),
// fxe (the clean-room source port), and — if it adopts this module — the
// fighters-legacy engine, instead of three separate implementations.
//
// A frame is `Begin(target, clear)` then one or more `Draw(mesh, camera, opts)`
// then `End()`. Targets are backend-owned offscreen surfaces: the OpenGL target
// exposes a native texture for zero-copy display; every target can `Read` its
// pixels into a CPU Image (for fxe validation, --render snapshots, and tests).
//
// The Software backend is context-free (no GPU, no window) — the only backend
// usable headless, and the substrate for the FA-faithful GG_/G_ rasteriser.
// The OpenGL backend (see fx_render/gl.h) matches the game's hardware path and
// requires a current GL context.
//
// MIT, clean-room from this project's own docs; see NOTICE.
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace fx_render {

// Object-space position + linear RGB colour. Grows later (normals, UVs) as the
// SH interpreter (fx_lib #279) supplies richer geometry.
struct Vertex {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float r = 1.0f, g = 1.0f, b = 1.0f;
};

// A vertex list interpreted per the draw's Primitive: as a triangle list (every
// 3 vertices) or a line list (every 2). Matches the interleaved upload the
// fxs preview already produces.
struct Mesh {
    std::vector<Vertex> vertices;
};

// Column-major 4x4 model-view-projection (element [col*4 + row]) — the OpenGL
// convention, so the GL and software backends share one matrix layout.
struct Camera {
    std::array<float, 16> mvp{{1, 0, 0, 0,
                               0, 1, 0, 0,
                               0, 0, 1, 0,
                               0, 0, 0, 1}};
};

// An owned RGBA8 CPU image (row-major, top-left origin). The software backend
// renders into one directly; any target can Read into one.
struct Image {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;  // width*height*4

    void resize(int w, int h);
    std::uint8_t* at(int x, int y);
    const std::uint8_t* at(int x, int y) const;
};

enum class Primitive {
    Triangles,  // every 3 vertices -> one filled triangle
    Lines,      // every 2 vertices -> one line segment
};

// Which backend produces the pixels.
enum class Backend {
    Software,  // context-free CPU rasteriser (always available)
    OpenGL,    // GPU path (requires a current GL context; see fx_render/gl.h)
};

struct DrawOptions {
    Primitive primitive = Primitive::Triangles;
    bool depth_test = true;
    bool depth_write = true;
    // Draw triangles as a depth-offset grey line overlay (the fxs wireframe
    // pass). Ignored for Primitive::Lines.
    bool wireframe = false;
    bool backface_cull = false;
};

// A backend-owned offscreen surface. Created by a Renderer, sized once.
class RenderTarget {
public:
    virtual ~RenderTarget() = default;
    virtual int width() const = 0;
    virtual int height() const = 0;
    // Copy the rendered pixels into `out` as RGBA8, top-left origin.
    virtual void Read(Image& out) const = 0;
    // Native handle for zero-copy display: the OpenGL colour-texture id, or 0
    // for the software backend (use Read + upload instead).
    virtual std::uintptr_t native_texture() const = 0;
};

// Draws meshes into a RenderTarget. Single-threaded; reuse across frames.
class Renderer {
public:
    virtual ~Renderer() = default;

    virtual std::unique_ptr<RenderTarget> MakeTarget(int width, int height) = 0;

    // Bind `target` and clear it to `clear` (RGBA). Pair with End().
    virtual void Begin(RenderTarget& target, const std::array<std::uint8_t, 4>& clear) = 0;
    virtual void Draw(const Mesh& mesh, const Camera& cam, const DrawOptions& opts) = 0;
    virtual void End() = 0;

    virtual Backend backend() const = 0;
};

// Construct the software renderer (always available). The OpenGL renderer is
// created via fx_render/gl.h (it needs a current GL context).
std::unique_ptr<Renderer> MakeRenderer(Backend backend);

// Convenience: render a single mesh into a CPU Image via a fresh target.
void RenderToImage(Renderer& r, const Mesh& mesh, const Camera& cam,
                   Image& out, const std::array<std::uint8_t, 4>& clear,
                   const DrawOptions& opts);

}  // namespace fx_render
