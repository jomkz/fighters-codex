// fx_render::fa — indexed surface, VGA palette, raster state (#328) and the
// fixed-16.16 YLR span core (#329).
// Clean-room from docs/fa/renderer.md §3.1, §7 + Key Global Reference.
#include "fx_render/fa.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>

#include "fx_render/render.h"

// The 16.16 helpers shift signed values; every supported toolchain (gcc,
// clang, msvc) implements arithmetic right shift, which C++17 leaves
// implementation-defined.
static_assert((-2 >> 1) == -1, "fa requires arithmetic right shift of signed values");

namespace fx_render {
namespace fa {

Surface::Surface(int width, int height) : Surface(width, height, width) {}

Surface::Surface(int width, int height, int stride) {
    width_ = std::max(0, width);
    height_ = std::max(0, height);
    stride_ = std::max(width_, stride);
    pixels_.assign(static_cast<std::size_t>(stride_) * height_, 0);
    rows_.resize(static_cast<std::size_t>(height_));
    for (int y = 0; y < height_; ++y) {
        rows_[static_cast<std::size_t>(y)] = pixels_.data() + static_cast<std::size_t>(y) * stride_;
    }
}

void Surface::Clear(std::uint8_t index) {
    std::fill(pixels_.begin(), pixels_.end(), index);
}

namespace {
// Conventional VGA 6-bit -> 8-bit widening (bit replication; 63 -> 255). Kept
// local so fx_render stays independent of fx_lib; the same convention lives in
// fx_lib as fx::pal_widen6 (#369).
std::uint8_t Widen6(std::uint8_t c6) {
    return static_cast<std::uint8_t>((c6 << 2) | (c6 >> 4));
}
}  // namespace

void Palette::Present(const Surface& src, Image& out) const {
    out.resize(src.width(), src.height());
    for (int y = 0; y < src.height(); ++y) {
        const std::uint8_t* in = src.row(y);
        std::uint8_t* px = out.at(0, y);
        for (int x = 0; x < src.width(); ++x) {
            const std::uint8_t idx = in[x];
            if (idx < kEntries) {
                const Rgb6& e = entries[idx];
                px[0] = Widen6(e.r);
                px[1] = Widen6(e.g);
                px[2] = Widen6(e.b);
            } else {
                px[0] = px[1] = px[2] = 0;
            }
            px[3] = 255;
            px += 4;
        }
    }
}

std::uint8_t Palette::Nearest(std::uint8_t r8, std::uint8_t g8, std::uint8_t b8) const {
    int best = 0;
    long best_d = -1;
    for (int i = 0; i < kEntries; ++i) {
        const long dr = static_cast<long>(Widen6(entries[i].r)) - r8;
        const long dg = static_cast<long>(Widen6(entries[i].g)) - g8;
        const long db = static_cast<long>(Widen6(entries[i].b)) - b8;
        const long d = dr * dr + dg * dg + db * db;
        if (best_d < 0 || d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return static_cast<std::uint8_t>(best);
}

Raster::Raster(Surface& target) : target_(&target) {
    // G_Init: the clip box opens to the full surface.
    clip_left_ = 0;
    clip_top_ = 0;
    clip_right_ = target.width() - 1;
    clip_bottom_ = target.height() - 1;
}

void Raster::SetClipBox(int left, int top, int right, int bottom) {
    clip_left_ = std::max(0, left);
    clip_top_ = std::max(0, top);
    clip_right_ = std::min(target_->width() - 1, right);
    clip_bottom_ = std::min(target_->height() - 1, bottom);
}

void Raster::Point(int x, int y) {
    if (x < clip_left_ || x > clip_right_ || y < clip_top_ || y > clip_bottom_) return;
    target_->row(y)[x] = color_;
}

void Raster::Rect(int left, int top, int right, int bottom) {
    left = std::max(left, clip_left_);
    top = std::max(top, clip_top_);
    right = std::min(right, clip_right_);
    bottom = std::min(bottom, clip_bottom_);
    for (int y = top; y <= bottom; ++y) {
        std::uint8_t* row = target_->row(y);
        for (int x = left; x <= right; ++x) row[x] = color_;
    }
}

namespace {
int FxCeil(Fx v) { return static_cast<int>((v + (kFxOne - 1)) >> 16); }
}  // namespace

bool Raster::PolygonToYlr(const PolyVertex* v, int n, YlrList& out) {
    if (!v || n < 3) return false;
    Fx y_min = v[0].y, y_max = v[0].y;
    for (int i = 1; i < n; ++i) {
        y_min = std::min(y_min, v[i].y);
        y_max = std::max(y_max, v[i].y);
    }
    const int top = FxCeil(y_min);
    const int bottom = FxCeil(y_max);  // half-open vertical coverage
    if (bottom <= top) return false;

    out.y_top = top;
    const std::size_t h = static_cast<std::size_t>(bottom - top);
    out.left.assign(h, std::numeric_limits<Fx>::max());
    out.right.assign(h, std::numeric_limits<Fx>::min());
    out.left_c.assign(h, 0);
    out.right_c.assign(h, 0);

    out.left_u.assign(h, 0);
    out.right_u.assign(h, 0);
    out.left_v.assign(h, 0);
    out.right_v.assign(h, 0);

    // Walk every non-horizontal edge, stepping its 16.16 x (and the packed
    // shade term c and texel u/v) per scanline; for a convex polygon min/max
    // per row is exactly the Left/Right pair, and the attributes follow
    // whichever edge holds each bound.
    for (int i = 0; i < n; ++i) {
        PolyVertex a = v[i];
        PolyVertex b = v[(i + 1) % n];
        if (a.y == b.y) continue;
        if (a.y > b.y) std::swap(a, b);
        const Fx dy_edge = b.y - a.y;
        const int ey0 = FxCeil(a.y), ey1 = FxCeil(b.y);
        for (int y = ey0; y < ey1; ++y) {
            const Fx dy = ToFx(y) - a.y;
            auto step = [&](Fx p0, Fx p1) {
                return p0 + static_cast<Fx>(
                    static_cast<std::int64_t>(dy) * (p1 - p0) / dy_edge);
            };
            const Fx x = step(a.x, b.x);
            const std::size_t row = static_cast<std::size_t>(y - top);
            if (x < out.left[row]) {
                out.left[row] = x;
                out.left_c[row] = step(a.c, b.c);
                out.left_u[row] = step(a.u, b.u);
                out.left_v[row] = step(a.v, b.v);
            }
            if (x > out.right[row]) {
                out.right[row] = x;
                out.right_c[row] = step(a.c, b.c);
                out.right_u[row] = step(a.u, b.u);
                out.right_v[row] = step(a.v, b.v);
            }
        }
    }
    return true;
}

std::uint8_t Texture::at(Fx u, Fx v) const {
    if (width <= 0 || height <= 0) return 0;
    const int tu = std::clamp(FxFloor(u), 0, width - 1);
    const int tv = std::clamp(FxFloor(v), 0, height - 1);
    return texels[static_cast<std::size_t>(tv) * width + tu];
}

void Raster::SetTmapRemap(const std::uint8_t* table256) {
    if (!table256) {
        remap_identity_ = true;
        return;
    }
    std::copy(table256, table256 + 256, tmap_remap_.begin());
    remap_identity_ = false;
}

void Raster::FillYlrFlat(const YlrList& ylr) {
    const int y0 = std::max(ylr.y_top, clip_top_);
    const int y1 = std::min(ylr.y_top + ylr.height() - 1, clip_bottom_);
    for (int y = y0; y <= y1; ++y) {
        const std::size_t row = static_cast<std::size_t>(y - ylr.y_top);
        if (ylr.left[row] > ylr.right[row]) continue;  // row untouched by any edge
        int xl = FxFloor(ylr.left[row]);
        int xr = FxFloor(ylr.right[row]);
        if (no_overlap_) --xr;
        xl = std::max(xl, clip_left_);
        xr = std::min(xr, clip_right_);
        if (xl > xr) continue;
        std::uint8_t* p = target_->row(y);
        for (int x = xl; x <= xr; ++x) p[x] = color_;
    }
}

void Raster::UPolygon(const PolyVertex* v, int n) {
    YlrList ylr;
    if (!PolygonToYlr(v, n, ylr)) return;
    FillYlrFlat(ylr);
}

void Raster::FillYlrShaded(const YlrList& ylr) {
    const int y0 = std::max(ylr.y_top, clip_top_);
    const int y1 = std::min(ylr.y_top + ylr.height() - 1, clip_bottom_);
    for (int y = y0; y <= y1; ++y) {
        const std::size_t row = static_cast<std::size_t>(y - ylr.y_top);
        const Fx x_l = ylr.left[row], x_r = ylr.right[row];
        if (x_l > x_r) continue;
        int xl = FxFloor(x_l);
        int xr = FxFloor(x_r);
        if (no_overlap_) --xr;
        xl = std::max(xl, clip_left_);
        xr = std::min(xr, clip_right_);
        if (xl > xr) continue;

        // c per one-pixel step; c is evaluated at each pixel's integer x
        // (inferred convention), so clip clamping needs no special-casing.
        const Fx span = x_r - x_l;
        const Fx dcdx = span > 0
            ? static_cast<Fx>(static_cast<std::int64_t>(ylr.right_c[row] - ylr.left_c[row]) *
                              kFxOne / span)
            : 0;
        Fx c = ylr.left_c[row] + static_cast<Fx>(
            static_cast<std::int64_t>(ToFx(xl) - x_l) * dcdx / kFxOne);
        std::uint8_t* p = target_->row(y);
        for (int x = xl; x <= xr; ++x, c += dcdx) {
            p[x] = static_cast<std::uint8_t>(std::clamp(FxFloor(c), 0, 255));
        }
    }
}

void Raster::SUPolygon(const PolyVertex* v, int n) {
    YlrList ylr;
    if (!PolygonToYlr(v, n, ylr)) return;
    FillYlrShaded(ylr);
}

void Raster::FillYlrTextured(const YlrList& ylr) {
    const int y0 = std::max(ylr.y_top, clip_top_);
    const int y1 = std::min(ylr.y_top + ylr.height() - 1, clip_bottom_);
    for (int y = y0; y <= y1; ++y) {
        const std::size_t row = static_cast<std::size_t>(y - ylr.y_top);
        const Fx x_l = ylr.left[row], x_r = ylr.right[row];
        if (x_l > x_r) continue;
        int xl = FxFloor(x_l);
        int xr = FxFloor(x_r);
        if (no_overlap_) --xr;
        xl = std::max(xl, clip_left_);
        xr = std::min(xr, clip_right_);
        if (xl > xr) continue;

        const Fx span = x_r - x_l;
        auto gradient = [&](Fx e_l, Fx e_r) {
            return span > 0 ? static_cast<Fx>(static_cast<std::int64_t>(e_r - e_l) *
                                              kFxOne / span)
                            : 0;
        };
        const Fx dudx = gradient(ylr.left_u[row], ylr.right_u[row]);
        const Fx dvdx = gradient(ylr.left_v[row], ylr.right_v[row]);
        const Fx dcdx = gradient(ylr.left_c[row], ylr.right_c[row]);
        const Fx off = ToFx(xl) - x_l;
        Fx u = ylr.left_u[row] + static_cast<Fx>(static_cast<std::int64_t>(off) * dudx / kFxOne);
        Fx v = ylr.left_v[row] + static_cast<Fx>(static_cast<std::int64_t>(off) * dvdx / kFxOne);
        Fx c = ylr.left_c[row] + static_cast<Fx>(static_cast<std::int64_t>(off) * dcdx / kFxOne);
        std::uint8_t* p = target_->row(y);
        for (int x = xl; x <= xr; ++x, u += dudx, v += dvdx, c += dcdx) {
            const std::uint8_t texel = texture_->at(u, v);
            // Palette index 0xFF is the transparent key (PIC.md). A transparent
            // texel shows the polygon's flat shade colour through it — except a
            // colour-0 (black) face is a pure decal overlay, whose transparent
            // texels must show the geometry behind (skip the pixel), not fill.
            if (texel == kTransparentTexel) {
                int ci = std::clamp(FxFloor(c), 0, 255);
                if (ci != 0) p[x] = static_cast<std::uint8_t>(ci);
            } else {
                p[x] = remap_identity_ ? texel : tmap_remap_[texel];
            }
        }
    }
}

void Raster::TexturedUPolygon(const PolyVertex* v, int n) {
    if (!texture_) {
        UPolygon(v, n);  // no binding: flat fallback
        return;
    }
    YlrList ylr;
    if (!PolygonToYlr(v, n, ylr)) return;
    FillYlrTextured(ylr);
}

void Raster::Polygon(const PolyVertex* v, int n) {
    switch (fill_type_) {
        case FillType::Shaded:
            SUPolygon(v, n);
            return;
        case FillType::Textured:
            TexturedUPolygon(v, n);
            return;
        case FillType::Flat:
            UPolygon(v, n);
            return;
    }
}

void Raster::TextureTriScan(const FVertex t[3], bool perspective) {
    if (!texture_) return;
    // Y-sorted float scanline walk over one triangle, matching the integer
    // path's conventions (integer scanlines, truncated inclusive spans).
    // The linear kernel interpolates u/v directly; the perspective kernel
    // interpolates u*rw, v*rw, rw and divides per pixel.
    const FVertex* s[3] = {&t[0], &t[1], &t[2]};
    std::sort(s, s + 3, [](const FVertex* a, const FVertex* b) { return a->y < b->y; });
    const float y_min = s[0]->y, y_mid = s[1]->y, y_max = s[2]->y;
    if (y_max <= y_min) return;

    const int top = std::max(static_cast<int>(std::ceil(y_min)), clip_top_);
    const int bottom = std::min(static_cast<int>(std::ceil(y_max)) - 1, clip_bottom_);

    struct Attr {
        float x, u, v, rw;
    };
    auto edge_at = [&](const FVertex& a, const FVertex& b, float fy) {
        const float tt = (fy - a.y) / (b.y - a.y);
        Attr r;
        r.x = a.x + (b.x - a.x) * tt;
        if (perspective) {
            r.u = a.u * a.rw + (b.u * b.rw - a.u * a.rw) * tt;
            r.v = a.v * a.rw + (b.v * b.rw - a.v * a.rw) * tt;
            r.rw = a.rw + (b.rw - a.rw) * tt;
        } else {
            r.u = a.u + (b.u - a.u) * tt;
            r.v = a.v + (b.v - a.v) * tt;
            r.rw = 1.0f;
        }
        return r;
    };

    for (int y = top; y <= bottom; ++y) {
        const float fy = static_cast<float>(y);
        Attr ea = edge_at(*s[0], *s[2], fy);  // the long edge
        // Scanlines below y_mid pair with the upper short edge, the rest
        // with the lower one; the integer scanline range makes both
        // denominators non-zero by construction.
        Attr eb = fy >= y_mid ? edge_at(*s[1], *s[2], fy) : edge_at(*s[0], *s[1], fy);
        if (eb.x < ea.x) std::swap(ea, eb);

        int xl = std::max(static_cast<int>(ea.x), clip_left_);
        int xr = std::min(static_cast<int>(eb.x), clip_right_);
        if (xl > xr) continue;

        const float dx = eb.x - ea.x;
        const float inv = dx > 0 ? 1.0f / dx : 0.0f;
        const float dudx = (eb.u - ea.u) * inv;
        const float dvdx = (eb.v - ea.v) * inv;
        const float drwdx = (eb.rw - ea.rw) * inv;
        float u = ea.u + (xl - ea.x) * dudx;
        float v = ea.v + (xl - ea.x) * dvdx;
        float rw = ea.rw + (xl - ea.x) * drwdx;

        std::uint8_t* p = target_->row(y);
        for (int x = xl; x <= xr; ++x, u += dudx, v += dvdx, rw += drwdx) {
            float su = u, sv = v;
            if (perspective) {
                if (rw <= 0.0f) continue;  // the carefulDiv guard
                su = u / rw;
                sv = v / rw;
            }
            const std::uint8_t texel = texture_->at(
                static_cast<Fx>(su * kFxOne), static_cast<Fx>(sv * kFxOne));
            p[x] = remap_identity_ ? texel : tmap_remap_[texel];
        }
    }
}

void Raster::TextureTriLinear(const FVertex t[3]) { TextureTriScan(t, false); }

void Raster::TextureTriPerspective(const FVertex t[3]) { TextureTriScan(t, true); }

namespace {

// One Sutherland–Hodgman pass against an axis-aligned plane. `Coord` picks
// the tested coordinate; `keep_below` selects which side is inside. Vertex
// attributes interpolate linearly at each crossing (int64 lerp in 16.16).
template <typename Coord>
void ClipEdge(const std::vector<PolyVertex>& in, std::vector<PolyVertex>& out,
              Fx bound, bool keep_below, Coord coord) {
    out.clear();
    const std::size_t n = in.size();
    for (std::size_t i = 0; i < n; ++i) {
        const PolyVertex& a = in[i];
        const PolyVertex& b = in[(i + 1) % n];
        const bool a_in = keep_below ? coord(a) <= bound : coord(a) >= bound;
        const bool b_in = keep_below ? coord(b) <= bound : coord(b) >= bound;
        if (a_in) out.push_back(a);
        if (a_in == b_in) continue;
        const std::int64_t num = static_cast<std::int64_t>(bound - coord(a));
        const std::int64_t den = static_cast<std::int64_t>(coord(b) - coord(a));
        auto lerp = [&](Fx pa, Fx pb) {
            return pa + static_cast<Fx>((static_cast<std::int64_t>(pb - pa) * num) / den);
        };
        PolyVertex c;
        c.x = lerp(a.x, b.x);
        c.y = lerp(a.y, b.y);
        c.u = lerp(a.u, b.u);
        c.v = lerp(a.v, b.v);
        c.c = lerp(a.c, b.c);
        out.push_back(c);
    }
}

}  // namespace

int Raster::ClipPolygon(const PolyVertex* in, int n, std::vector<PolyVertex>& out) const {
    out.clear();
    if (!in || n < 3) return 0;
    std::vector<PolyVertex> a(in, in + n), b;
    // clip_edge_left / _right / _top / _bottom — the render-core order. The
    // right/bottom planes sit at the last representable 16.16 value inside
    // the boundary pixel (the clip bounds are inclusive).
    ClipEdge(a, b, ToFx(clip_left_), false, [](const PolyVertex& p) { return p.x; });
    ClipEdge(b, a, ToFx(clip_right_) + (kFxOne - 1), true, [](const PolyVertex& p) { return p.x; });
    ClipEdge(a, b, ToFx(clip_top_), false, [](const PolyVertex& p) { return p.y; });
    ClipEdge(b, a, ToFx(clip_bottom_) + (kFxOne - 1), true, [](const PolyVertex& p) { return p.y; });
    if (a.size() < 3) return 0;
    out = std::move(a);
    return static_cast<int>(out.size());
}

void Raster::PolygonClipped(const PolyVertex* v, int n) {
    std::vector<PolyVertex> clipped;
    if (ClipPolygon(v, n, clipped) < 3) return;
    Polygon(clipped.data(), static_cast<int>(clipped.size()));
}

namespace {
// Cohen–Sutherland outcodes for the integer clip box.
constexpr unsigned kOutLeft = 1, kOutRight = 2, kOutTop = 4, kOutBottom = 8;
unsigned OutCode(int x, int y, int l, int t, int r, int b) {
    unsigned code = 0;
    if (x < l) code |= kOutLeft;
    if (x > r) code |= kOutRight;
    if (y < t) code |= kOutTop;
    if (y > b) code |= kOutBottom;
    return code;
}
}  // namespace

bool Raster::ClipLine(int& x0, int& y0, int& x1, int& y1) const {
    const int l = clip_left_, t = clip_top_, r = clip_right_, b = clip_bottom_;
    unsigned c0 = OutCode(x0, y0, l, t, r, b);
    unsigned c1 = OutCode(x1, y1, l, t, r, b);
    while (true) {
        if (!(c0 | c1)) return true;   // trivially accepted
        if (c0 & c1) return false;     // trivially rejected
        const unsigned out = c0 ? c0 : c1;
        std::int64_t x = 0, y = 0;
        if (out & kOutLeft) {
            y = y0 + static_cast<std::int64_t>(y1 - y0) * (l - x0) / (x1 - x0);
            x = l;
        } else if (out & kOutRight) {
            y = y0 + static_cast<std::int64_t>(y1 - y0) * (r - x0) / (x1 - x0);
            x = r;
        } else if (out & kOutTop) {
            x = x0 + static_cast<std::int64_t>(x1 - x0) * (t - y0) / (y1 - y0);
            y = t;
        } else {
            x = x0 + static_cast<std::int64_t>(x1 - x0) * (b - y0) / (y1 - y0);
            y = b;
        }
        if (out == c0) {
            x0 = static_cast<int>(x);
            y0 = static_cast<int>(y);
            c0 = OutCode(x0, y0, l, t, r, b);
        } else {
            x1 = static_cast<int>(x);
            y1 = static_cast<int>(y);
            c1 = OutCode(x1, y1, l, t, r, b);
        }
    }
}

void Raster::Line(int x0, int y0, int x1, int y1) {
    if (!ClipLine(x0, y0, x1, y1)) return;
    // Integer line walk between the clipped endpoints (inclusive).
    const int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0);
    const int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        target_->row(y0)[x0] = color_;
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

unsigned CodePnt(const ClipVertex& v, float near_w) {
    return v.w < near_w ? kClipNear : 0;
}

bool PaintersList::DrawsBefore(const Key& a, const Key& b) {
    return static_cast<std::int64_t>(a.depth) + a.size >
           static_cast<std::int64_t>(b.depth) + b.size;
}

void PaintersList::Add(Key key, std::function<void(Raster&)> draw) {
    items_.push_back({key, std::move(draw)});
}

void PaintersList::Flush(Raster& r) {
    std::stable_sort(items_.begin(), items_.end(),
                     [](const Item& a, const Item& b) { return DrawsBefore(a.key, b.key); });
    for (Item& it : items_) it.draw(r);
    items_.clear();
}

int NearClipPolygon(const ClipVertex* in, int n, float near_w, ClipVertex* out) {
    if (!in || !out || n < 3) return 0;
    unsigned and_code = ~0u, or_code = 0;
    for (int i = 0; i < n; ++i) {
        const unsigned c = CodePnt(in[i], near_w);
        and_code &= c;
        or_code |= c;
    }
    if (and_code) return 0;  // trivial reject: every vertex behind the plane
    if (!or_code) {          // trivial accept: no guard bit set
        for (int i = 0; i < n; ++i) out[i] = in[i];
        return n;
    }
    int m = 0;
    for (int i = 0; i < n; ++i) {
        const ClipVertex& a = in[i];
        const ClipVertex& b = in[(i + 1) % n];
        const bool a_in = a.w >= near_w;
        const bool b_in = b.w >= near_w;
        if (a_in) out[m++] = a;
        if (a_in == b_in) continue;
        const float t = (near_w - a.w) / (b.w - a.w);
        ClipVertex c;
        c.x = a.x + (b.x - a.x) * t;
        c.y = a.y + (b.y - a.y) * t;
        c.z = a.z + (b.z - a.z) * t;
        c.w = near_w;
        c.u = a.u + (b.u - a.u) * t;
        c.v = a.v + (b.v - a.v) * t;
        c.c = a.c + (b.c - a.c) * t;
        out[m++] = c;
    }
    return m;
}

}  // namespace fa
}  // namespace fx_render
