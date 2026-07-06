// fx_render::fa — indexed surface, VGA palette, raster state (#328) and the
// fixed-16.16 YLR span core (#329).
// Clean-room from docs/fa/renderer.md §3.1, §7 + Key Global Reference.
#include "fx_render/fa.h"

#include <algorithm>
#include <cstdint>
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
// Conventional VGA 6-bit -> 8-bit widening.
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

    // Walk every non-horizontal edge, stepping its 16.16 x per scanline;
    // for a convex polygon min/max per row is exactly the Left/Right pair.
    for (int i = 0; i < n; ++i) {
        Fx x0 = v[i].x, y0 = v[i].y;
        Fx x1 = v[(i + 1) % n].x, y1 = v[(i + 1) % n].y;
        if (y0 == y1) continue;
        if (y0 > y1) {
            std::swap(x0, x1);
            std::swap(y0, y1);
        }
        const int ey0 = FxCeil(y0), ey1 = FxCeil(y1);
        for (int y = ey0; y < ey1; ++y) {
            const std::int64_t num =
                static_cast<std::int64_t>(ToFx(y) - y0) * (x1 - x0);
            const Fx x = x0 + static_cast<Fx>(num / (y1 - y0));
            const std::size_t row = static_cast<std::size_t>(y - top);
            out.left[row] = std::min(out.left[row], x);
            out.right[row] = std::max(out.right[row], x);
        }
    }
    return true;
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

}  // namespace fa
}  // namespace fx_render
