// fx_render — FA-faithful software backend: the generic Renderer API driving
// the fa pipeline (#334). See fa_backend.h for the fidelity boundary.
#include "fx_render/fa_backend.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace fx_render {
namespace {

// The near plane for NPM clipping in camera w; small enough not to shave
// visible geometry, large enough to keep the projection divide stable.
constexpr float kNearW = 1e-3f;

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

std::uint8_t ToU8(float c) {
    return static_cast<std::uint8_t>(std::lround(std::clamp(c, 0.0f, 1.0f) * 255.0f));
}

fa::Fx ToFxF(float v) {
    return static_cast<fa::Fx>(std::lround(
        std::clamp(v, -30000.0f, 30000.0f) * static_cast<float>(fa::kFxOne)));
}

class FaTarget final : public RenderTarget {
public:
    FaTarget(int w, int h, const fa::Palette* pal)
        : surface(std::max(1, w), std::max(1, h)), pal_(pal) {}
    int width() const override { return surface.width(); }
    int height() const override { return surface.height(); }
    void Read(Image& out) const override { pal_->Present(surface, out); }
    std::uintptr_t native_texture() const override { return 0; }

    fa::Surface surface;

private:
    const fa::Palette* pal_;
};

class FaRenderer final : public Renderer {
public:
    explicit FaRenderer(fa::Palette pal) : pal_(std::move(pal)) {
        grey_ = pal_.Nearest(179, 179, 179);  // the wireframe overlay colour
    }

    std::unique_ptr<RenderTarget> MakeTarget(int width, int height) override {
        return std::make_unique<FaTarget>(width, height, &pal_);
    }

    void Begin(RenderTarget& target, const std::array<std::uint8_t, 4>& clear) override {
        cur_ = static_cast<FaTarget*>(&target);
        raster_ = std::make_unique<fa::Raster>(cur_->surface);
        cur_->surface.Clear(pal_.Nearest(clear[0], clear[1], clear[2]));
    }

    void Draw(const Mesh& mesh, const Camera& cam, const DrawOptions& opts) override {
        if (!cur_ || mesh.vertices.empty()) return;
        // Queue under the object-level painter's key (GRAddBrentObj): the
        // mean camera-space depth of the mesh; the draw itself runs at
        // End() in back-to-front order. Submission order breaks ties, so
        // the preview's solid-then-wireframe pass order is preserved.
        float sum_w = 0.0f;
        for (const Vertex& v : mesh.vertices) {
            sum_w += std::max(0.0f, Transform(cam.mvp, v).w);
        }
        const float mean_w = sum_w / static_cast<float>(mesh.vertices.size());
        list_.Add({ToFxF(mean_w), 0},
                  [this, mesh, cam, opts](fa::Raster& r) { DrawNow(mesh, cam, opts, r); });
    }

    void End() override {
        if (!cur_) return;
        list_.Flush(*raster_);
        raster_.reset();
        cur_ = nullptr;
    }

    Backend backend() const override { return Backend::Software; }

private:
    void DrawNow(const Mesh& mesh, const Camera& cam, const DrawOptions& opts,
                 fa::Raster& r) {
        const int w = cur_->surface.width(), h = cur_->surface.height();
        const fa::Texture* tex =
            (mesh.texture && mesh.texture->width > 0 && mesh.texture->height > 0)
                ? QuantizedTexture(mesh.texture.get())
                : nullptr;

        auto to_clip = [&](const Vertex& src) {
            const Vec4 c = Transform(cam.mvp, src);
            fa::ClipVertex cv;
            cv.x = c.x;
            cv.y = c.y;
            cv.z = c.z;
            cv.w = c.w;
            cv.u = src.u * static_cast<float>(tex ? tex->width : 0);
            cv.v = src.v * static_cast<float>(tex ? tex->height : 0);
            cv.c = static_cast<float>(pal_.Nearest(ToU8(src.r), ToU8(src.g), ToU8(src.b)));
            return cv;
        };
        auto project = [&](const fa::ClipVertex& cv) {
            const float inv = 1.0f / cv.w;
            fa::PolyVertex p;
            p.x = ToFxF((cv.x * inv * 0.5f + 0.5f) * static_cast<float>(w));
            p.y = ToFxF((1.0f - (cv.y * inv * 0.5f + 0.5f)) * static_cast<float>(h));
            p.u = ToFxF(cv.u);
            p.v = ToFxF(cv.v);
            p.c = ToFxF(cv.c);
            return p;
        };
        auto line_px = [&](const fa::PolyVertex& a, const fa::PolyVertex& b,
                           std::uint8_t colour) {
            r.SetColor(colour);
            r.Line(fa::FxFloor(a.x), fa::FxFloor(a.y), fa::FxFloor(b.x), fa::FxFloor(b.y));
        };

        if (opts.primitive == Primitive::Lines) {
            for (std::size_t i = 0; i + 1 < mesh.vertices.size(); i += 2) {
                fa::ClipVertex seg[2] = {to_clip(mesh.vertices[i]),
                                         to_clip(mesh.vertices[i + 1])};
                if (seg[0].w < kNearW || seg[1].w < kNearW) continue;
                line_px(project(seg[0]), project(seg[1]),
                        static_cast<std::uint8_t>(seg[0].c));
            }
            return;
        }

        for (std::size_t t = 0; t + 2 < mesh.vertices.size(); t += 3) {
            const fa::ClipVertex in[3] = {to_clip(mesh.vertices[t]),
                                          to_clip(mesh.vertices[t + 1]),
                                          to_clip(mesh.vertices[t + 2])};
            fa::ClipVertex clipped[4];
            const int n = fa::NearClipPolygon(in, 3, kNearW, clipped);
            if (n < 3) continue;

            fa::PolyVertex poly[4];
            for (int i = 0; i < n; ++i) poly[i] = project(clipped[i]);

            if (opts.wireframe) {  // grey painter-order overlay, like the GL pass
                for (int k = 0; k < n; ++k) line_px(poly[k], poly[(k + 1) % n], grey_);
                continue;
            }
            if (opts.backface_cull) {
                const std::int64_t area =
                    static_cast<std::int64_t>(poly[1].x - poly[0].x) * (poly[2].y - poly[0].y) -
                    static_cast<std::int64_t>(poly[1].y - poly[0].y) * (poly[2].x - poly[0].x);
                if (area < 0) continue;
            }
            if (tex) {
                r.SetTexture(tex);
                r.SetFillType(fa::FillType::Textured);
            } else {
                r.SetFillType(fa::FillType::Shaded);
            }
            r.PolygonClipped(poly, n);
        }
        r.SetTexture(nullptr);
    }

    // Quantize an RGBA mesh texture to indexed texels once and cache it by
    // source pointer (re-quantized if the dimensions change; the preview
    // swaps textures by replacing the Image, not mutating it in place).
    const fa::Texture* QuantizedTexture(const Image* img) {
        fa::Texture& slot = tex_cache_[img];
        if (slot.width != img->width || slot.height != img->height) {
            slot.width = img->width;
            slot.height = img->height;
            slot.texels.resize(static_cast<std::size_t>(img->width) * img->height);
            for (int y = 0; y < img->height; ++y) {
                const std::uint8_t* px = img->at(0, y);
                for (int x = 0; x < img->width; ++x, px += 4) {
                    slot.texels[static_cast<std::size_t>(y) * img->width + x] =
                        pal_.Nearest(px[0], px[1], px[2]);
                }
            }
        }
        return &slot;
    }

    fa::Palette pal_;
    std::uint8_t grey_ = 0;
    FaTarget* cur_ = nullptr;
    std::unique_ptr<fa::Raster> raster_;
    fa::PaintersList list_;
    std::map<const Image*, fa::Texture> tex_cache_;
};

}  // namespace

std::unique_ptr<Renderer> MakeFaRenderer(fa::Palette palette) {
    return std::make_unique<FaRenderer>(std::move(palette));
}

}  // namespace fx_render
