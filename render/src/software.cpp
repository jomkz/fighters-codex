// fx_render — context-free software rasteriser.
//
// A minimal but correct Gouraud triangle rasteriser: transform by the MVP,
// perspective-divide to NDC, map to the viewport (top-left origin), then fill
// with a barycentric inside-test and a depth buffer. This is the foundation of
// the FA-faithful GG_/G_ software path (fx_render #290); it deliberately owns
// no GPU state so it runs headless (tests, fxc validation).
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

// Signed area of the edge (a -> b) w.r.t. point p (twice the triangle area).
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
};

class Software final : public Renderer {
public:
    void Render(const Mesh& mesh, const Camera& cam, Image& target,
                const RenderOptions& opts) override {
        const int w = target.width;
        const int h = target.height;
        if (w <= 0 || h <= 0) return;
        if (target.pixels.size() != static_cast<std::size_t>(w) * h * 4) {
            target.resize(w, h);
        }

        // Clear.
        for (int i = 0; i < w * h; ++i) {
            std::uint8_t* p = &target.pixels[static_cast<std::size_t>(i) * 4];
            p[0] = opts.clear[0];
            p[1] = opts.clear[1];
            p[2] = opts.clear[2];
            p[3] = opts.clear[3];
        }

        std::vector<float> depth(static_cast<std::size_t>(w) * h,
                                 std::numeric_limits<float>::infinity());

        for (std::size_t t = 0; t + 2 < mesh.vertices.size(); t += 3) {
            ScreenVtx v[3];
            bool behind = false;
            for (int k = 0; k < 3; ++k) {
                const Vertex& src = mesh.vertices[t + k];
                Vec4 c = Transform(cam.mvp, src);
                if (c.w <= 0.0f) {  // simple near-plane reject; full clip is #290
                    behind = true;
                    break;
                }
                float inv = 1.0f / c.w;
                float ndc_x = c.x * inv, ndc_y = c.y * inv, ndc_z = c.z * inv;
                v[k].x = (ndc_x * 0.5f + 0.5f) * w;
                v[k].y = (1.0f - (ndc_y * 0.5f + 0.5f)) * h;  // flip: top-left origin
                v[k].z = ndc_z;
                v[k].r = src.r;
                v[k].g = src.g;
                v[k].b = src.b;
            }
            if (behind) continue;

            if (opts.wireframe) {
                DrawLine(target, v[0], v[1]);
                DrawLine(target, v[1], v[2]);
                DrawLine(target, v[2], v[0]);
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
                    if (z >= depth[di]) continue;
                    depth[di] = z;

                    std::uint8_t* p = &target.pixels[di * 4];
                    p[0] = ToU8(b0 * v[0].r + b1 * v[1].r + b2 * v[2].r);
                    p[1] = ToU8(b0 * v[0].g + b1 * v[1].g + b2 * v[2].g);
                    p[2] = ToU8(b0 * v[0].b + b1 * v[1].b + b2 * v[2].b);
                    p[3] = 255;
                }
            }
        }
    }

    Backend backend() const override { return Backend::Software; }

private:
    // Flat-colour DDA line (wireframe uses vertex a's colour).
    static void DrawLine(Image& img, const ScreenVtx& a, const ScreenVtx& b) {
        float dx = b.x - a.x, dy = b.y - a.y;
        int steps = static_cast<int>(std::ceil(std::max(std::fabs(dx), std::fabs(dy))));
        if (steps <= 0) steps = 1;
        std::uint8_t cr = ToU8(a.r), cg = ToU8(a.g), cb = ToU8(a.b);
        for (int i = 0; i <= steps; ++i) {
            float f = static_cast<float>(i) / steps;
            int x = static_cast<int>(std::lround(a.x + dx * f));
            int y = static_cast<int>(std::lround(a.y + dy * f));
            if (x < 0 || y < 0 || x >= img.width || y >= img.height) continue;
            std::uint8_t* p = img.at(x, y);
            p[0] = cr;
            p[1] = cg;
            p[2] = cb;
            p[3] = 255;
        }
    }
};

}  // namespace

std::unique_ptr<Renderer> MakeSoftwareRenderer() {
    return std::make_unique<Software>();
}

}  // namespace fx_render
