// fx_render — a shared, backend-agnostic renderer for the FA reconstruction.
//
// One geometry->pixels API with interchangeable backends, so the OpenGL and
// software render paths are built once and shared by fx-gui (the asset viewer),
// fxc (the clean-room source port), and — if it adopts this module — the
// fighters-legacy engine, instead of three separate implementations.
//
// The Software backend is context-free (no GPU, no window): it is the only
// backend usable in headless tests and the substrate for the FA-faithful
// GG_/G_ rasteriser. The OpenGL backend (fx_render #289) matches the game's
// hardware path and assumes a current GL context.
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

// A non-indexed triangle list: every three vertices form one triangle. This
// matches the interleaved upload the fx-gui SH preview already produces; an
// indexed path can be added without changing the backends' contract.
struct Mesh {
    std::vector<Vertex> vertices;
};

// Column-major 4x4 model-view-projection (element [col*4 + row]) — the OpenGL
// convention, so the extracted GL backend and the software backend agree on a
// single matrix layout.
struct Camera {
    std::array<float, 16> mvp{{1, 0, 0, 0,
                               0, 1, 0, 0,
                               0, 0, 1, 0,
                               0, 0, 0, 1}};
};

// An owned RGBA8 offscreen target (row-major, top-left origin). Owning its
// pixels is what lets the software backend render with no GPU.
struct Image {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;  // width*height*4

    // Allocate/clear storage for a w x h image.
    void resize(int w, int h);
    // Pointer to the 4 bytes of pixel (x, y); no bounds checking.
    std::uint8_t* at(int x, int y);
    const std::uint8_t* at(int x, int y) const;
};

struct RenderOptions {
    bool wireframe = false;           // draw triangle edges instead of filling
    bool backface_cull = false;       // drop clockwise (negative-area) triangles
    std::array<std::uint8_t, 4> clear{{0, 0, 0, 255}};  // RGBA background
};

// Which backend produces the pixels.
enum class Backend {
    Software,  // context-free CPU rasteriser (always available)
    OpenGL,    // GPU path (requires a current GL context)
};

// Draws meshes into an Image. A Renderer is single-threaded and reusable.
class Renderer {
public:
    virtual ~Renderer() = default;

    // Render `mesh` seen through `cam` into `target`, honouring `opts`. The
    // target is resized to hold width*height*4 bytes if it is not already.
    virtual void Render(const Mesh& mesh, const Camera& cam, Image& target,
                        const RenderOptions& opts) = 0;

    virtual Backend backend() const = 0;
};

// Construct a renderer for `backend`. Software is always available. OpenGL
// returns nullptr until the GL backend is wired (fx_render #289); callers
// should fall back to Software.
std::unique_ptr<Renderer> MakeRenderer(Backend backend);

}  // namespace fx_render
