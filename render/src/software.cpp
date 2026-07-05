// fx_render — context-free software rasteriser.
//
// A minimal but correct Gouraud triangle rasteriser plus line drawing:
// transform by the MVP, perspective-divide to NDC, map to the viewport
// (top-left origin), then fill with a barycentric inside-test and a shared
// depth buffer. This is the foundation of the FA-faithful GG_/G_ software path
// (fx_render #290); it owns no GPU state so it runs headless (tests, fxe).
#include "fx_render/render.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace fx_render {
namespace {

struct Vec4 {
    float x, y, z, w;
};

// Column-major mat4 (element [col*4 + row]) times an object-space position.
Vec4 Transform(const std::array<float, 16>& m, const Vertex& v) {
    return {m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12],
            m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13],
            m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14],
            m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15]};
}

// Signed area of edge (a -> b) w.r.t. point p (twice the triangle area).
float Edge(float ax, float ay, float bx, float by, float px, float py) {
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

std::uint8_t ToU8(float c) {
    return static_cast<std::uint8_t>(std::lround(std::clamp(c, 0.0f, 1.0f) * 255.0f));
}

// A screen-space vertex after transform + perspective divide.
struct ScreenVtx {
    float x, y, z;  // pixels, pixels, NDC depth
    float r, g, b;
    bool clipped;   // behind the near plane
};

class SoftwareTarget final : public RenderTarget {
public:
    SoftwareTarget(int w, int h) {
        image.resize(w, h);
        depth.assign(static_cast<std::size_t>(image.width) * image.height, 0.0f);
    }
    int width() const override { return image.width; }
    int height() const override { return image.height; }
    void Read(Image& out) const override { out = image; }
    std::uintptr_t native_texture() const override { return 0; }

    Image image;
    std::vector<float> depth;
};

class Software final : public Renderer {
public:
    std::unique_ptr<RenderTarget> MakeTarget(int width, int height) override {
        return std::make_unique<SoftwareTarget>(width, height);
    }

    void Begin(RenderTarget& target, const std::array<std::uint8_t, 4>& clear) override {
        cur_ = static_cast<SoftwareTarget*>(&target);
        const int n = cur_->image.width * cur_->image.height;
        for (int i = 0; i < n; ++i) {
            std::uint8_t* p = &cur_->image.pixels[static_cast<std::size_t>(i) * 4];
            p[0] = clear[0];
            p[1] = clear[1];
            p[2] = clear[2];
            p[3] = clear[3];
        }
        std::fill(cur_->depth.begin(), cur_->depth.end(),
                  std::numeric_limits<float>::infinity());
    }

    void Draw(const Mesh& mesh, const Camera& cam, const DrawOptions& opts) override {
        if (!cur_) return;
        const int w = cur_->image.width, h = cur_->image.height;
        if (w <= 0 || h <= 0) return;

        auto project = [&](const Vertex& src) {
            ScreenVtx s;
            Vec4 c = Transform(cam.mvp, src);
            s.clipped = (c.w <= 0.0f);  // full clip is #290
            float inv = s.clipped ? 1.0f : 1.0f / c.w;
            s.x = (c.x * inv * 0.5f + 0.5f) * w;
            s.y = (1.0f - (c.y * inv * 0.5f + 0.5f)) * h;  // flip: top-left origin
            s.z = c.z * inv;
            s.r = src.r;
            s.g = src.g;
            s.b = src.b;
            return s;
        };

        if (opts.primitive == Primitive::Lines) {
            for (std::size_t i = 0; i + 1 < mesh.vertices.size(); i += 2) {
                ScreenVtx a = project(mesh.vertices[i]);
                ScreenVtx b = project(mesh.vertices[i + 1]);
                if (a.clipped || b.clipped) continue;
                DrawLine(a, b);
            }
            return;
        }

        for (std::size_t t = 0; t + 2 < mesh.vertices.size(); t += 3) {
            ScreenVtx v[3] = {project(mesh.vertices[t]), project(mesh.vertices[t + 1]),
                              project(mesh.vertices[t + 2])};
            if (v[0].clipped || v[1].clipped || v[2].clipped) continue;

            if (opts.wireframe) {  // grey line overlay, no depth write
                for (int k = 0; k < 3; ++k) {
                    ScreenVtx a = v[k], b = v[(k + 1) % 3];
                    a.r = a.g = a.b = 0.7f;
                    b.r = b.g = b.b = 0.7f;
                    DrawLine(a, b);
                }
                continue;
            }

            float area = Edge(v[0].x, v[0].y, v[1].x, v[1].y, v[2].x, v[2].y);
            if (area == 0.0f) continue;
            if (opts.backface_cull && area < 0.0f) continue;

            int min_x = std::max(0, static_cast<int>(std::floor(std::min({v[0].x, v[1].x, v[2].x}))));
            int max_x = std::min(w - 1, static_cast<int>(std::ceil(std::max({v[0].x, v[1].x, v[2].x}))));
            int min_y = std::max(0, static_cast<int>(std::floor(std::min({v[0].y, v[1].y, v[2].y}))));
            int max_y = std::min(h - 1, static_cast<int>(std::ceil(std::max({v[0].y, v[1].y, v[2].y}))));

            for (int y = min_y; y <= max_y; ++y) {
                for (int x = min_x; x <= max_x; ++x) {
                    float px = x + 0.5f, py = y + 0.5f;
                    float w0 = Edge(v[1].x, v[1].y, v[2].x, v[2].y, px, py);
                    float w1 = Edge(v[2].x, v[2].y, v[0].x, v[0].y, px, py);
                    float w2 = Edge(v[0].x, v[0].y, v[1].x, v[1].y, px, py);
                    bool inside = (w0 >= 0 && w1 >= 0 && w2 >= 0) ||
                                  (w0 <= 0 && w1 <= 0 && w2 <= 0);
                    if (!inside) continue;

                    float b0 = w0 / area, b1 = w1 / area, b2 = w2 / area;
                    float z = b0 * v[0].z + b1 * v[1].z + b2 * v[2].z;
                    std::size_t di = static_cast<std::size_t>(y) * w + x;
                    if (opts.depth_test && z >= cur_->depth[di]) continue;
                    if (opts.depth_write) cur_->depth[di] = z;

                    std::uint8_t* p = &cur_->image.pixels[di * 4];
                    p[0] = ToU8(b0 * v[0].r + b1 * v[1].r + b2 * v[2].r);
                    p[1] = ToU8(b0 * v[0].g + b1 * v[1].g + b2 * v[2].g);
                    p[2] = ToU8(b0 * v[0].b + b1 * v[1].b + b2 * v[2].b);
                    p[3] = 255;
                }
            }
        }
    }

    void End() override { cur_ = nullptr; }
    Backend backend() const override { return Backend::Software; }

private:
    void DrawLine(const ScreenVtx& a, const ScreenVtx& b) {
        const int w = cur_->image.width, h = cur_->image.height;
        float dx = b.x - a.x, dy = b.y - a.y;
        int steps = static_cast<int>(std::ceil(std::max(std::fabs(dx), std::fabs(dy))));
        if (steps <= 0) steps = 1;
        for (int i = 0; i <= steps; ++i) {
            float f = static_cast<float>(i) / steps;
            int x = static_cast<int>(std::lround(a.x + dx * f));
            int y = static_cast<int>(std::lround(a.y + dy * f));
            if (x < 0 || y < 0 || x >= w || y >= h) continue;
            std::uint8_t* p = cur_->image.at(x, y);
            p[0] = ToU8(a.r);
            p[1] = ToU8(a.g);
            p[2] = ToU8(a.b);
            p[3] = 255;
        }
    }

    SoftwareTarget* cur_ = nullptr;
};

}  // namespace

std::unique_ptr<Renderer> MakeSoftwareRenderer() {
    return std::make_unique<Software>();
}

}  // namespace fx_render
