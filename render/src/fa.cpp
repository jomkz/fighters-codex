// fx_render::fa — indexed surface, VGA palette, raster state (#328).
// Clean-room from docs/fa/renderer.md §7 + Key Global Reference.
#include "fx_render/fa.h"

#include <algorithm>

#include "fx_render/render.h"

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

}  // namespace fa
}  // namespace fx_render
