#include "terrain_preview.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <unordered_map>

namespace fxg {

namespace {

// Fixed directional light, render space (X=right, Y=up, Z=forward) — matches
// the SH preview's key light so terrain and models read the same way.
constexpr float kLx = 0.40f, kLy = 0.82f, kLz = -0.40f;
constexpr float kAmbient = 0.35f, kDiffuse = 0.65f;

float LeafElevation(const fx::T2Map& map, int x, int y) {
    x = std::clamp(x, 0, (int)map.leaves_w - 1);
    y = std::clamp(y, 0, (int)map.leaves_h - 1);
    return (float)map.leaves[(std::size_t)y * map.leaves_w + x].elevation;
}

// Height at a grid vertex = average of the (up to four) leaves meeting there,
// so adjacent cells share corner heights and the field is continuous.
float VertexHeight(const fx::T2Map& map, int vx, int vy) {
    float h = LeafElevation(map, vx - 1, vy - 1) + LeafElevation(map, vx, vy - 1) +
              LeafElevation(map, vx - 1, vy) + LeafElevation(map, vx, vy);
    return h * 0.25f;
}

}  // namespace

TerrainScene BuildTerrainScene(const fx::T2Map& map, const TileProvider& tiles,
                               const TerrainBuildOptions& opts) {
    TerrainScene scene;
    if (map.leaves_w == 0 || map.leaves_h == 0 ||
        map.leaves.size() != (std::size_t)map.leaves_w * map.leaves_h)
        return scene;

    const int W = (int)map.leaves_w, H = (int)map.leaves_h;
    const float cx = W * opts.xz_scale * 0.5f;
    const float cz = H * opts.xz_scale * 0.5f;

    // World position of grid vertex (vx, vy), centred on the origin in XZ.
    auto pos = [&](int vx, int vy, float out[3]) {
        out[0] = vx * opts.xz_scale - cx;
        out[1] = VertexHeight(map, vx, vy) * opts.height_scale;
        out[2] = vy * opts.xz_scale - cz;
    };

    // One textured mesh per variant that resolves to a tile.
    std::unordered_map<std::uint8_t, std::size_t> variant_slot;
    std::unordered_map<std::uint8_t, std::shared_ptr<const fx_render::Image>> tile_cache;
    auto tile_for = [&](std::uint8_t v) -> std::shared_ptr<const fx_render::Image> {
        auto it = tile_cache.find(v);
        if (it != tile_cache.end()) return it->second;
        auto img = tiles ? tiles(v) : nullptr;
        tile_cache.emplace(v, img);
        return img;
    };

    float min_h = 1e30f, max_h = -1e30f;

    for (int ly = 0; ly < H; ++ly) {
        for (int lx = 0; lx < W; ++lx) {
            const fx::T2Record& leaf = map.leaves[(std::size_t)ly * W + lx];
            const bool water = leaf.surface_class == 0xFF;
            if (water) scene.water_leaves++;
            else       scene.land_leaves++;

            // Four cell corners (render space) and their shared heights.
            float p00[3], p10[3], p11[3], p01[3];
            pos(lx,     ly,     p00);
            pos(lx + 1, ly,     p10);
            pos(lx + 1, ly + 1, p11);
            pos(lx,     ly + 1, p01);
            for (float* p : {p00, p10, p11, p01}) {
                min_h = std::min(min_h, p[1]);
                max_h = std::max(max_h, p[1]);
            }

            // Flat directional shade from the cell's geometric normal.
            float ax = p10[0]-p00[0], ay = p10[1]-p00[1], az = p10[2]-p00[2];
            float bx = p01[0]-p00[0], by = p01[1]-p00[1], bz = p01[2]-p00[2];
            float nx = ay*bz - az*by, ny = az*bx - ax*bz, nz = ax*by - ay*bx;
            float nl = std::sqrt(nx*nx + ny*ny + nz*nz);
            float shade = 1.0f;
            if (opts.shade && nl > 0.0f) {
                float d = (nx*kLx + ny*kLy + nz*kLz) / nl;
                shade = kAmbient + kDiffuse * std::max(0.0f, d);
            }

            std::shared_ptr<const fx_render::Image> tile =
                water ? nullptr : tile_for(leaf.texture_variant);

            if (tile) {
                scene.textured_leaves++;
                std::size_t slot;
                auto it = variant_slot.find(leaf.texture_variant);
                if (it == variant_slot.end()) {
                    slot = scene.textured.size();
                    variant_slot.emplace(leaf.texture_variant, slot);
                    scene.textured.emplace_back();
                    scene.textured.back().texture = tile;
                } else {
                    slot = it->second;
                }
                auto& vs = scene.textured[slot].vertices;
                // Full tile per leaf; texture sampling ignores vertex colour.
                auto push = [&](const float* p, float u, float v) {
                    vs.push_back({p[0], p[1], p[2], 1.0f, 1.0f, 1.0f, u, v});
                };
                push(p00, 0,0); push(p10, 1,0); push(p11, 1,1);
                push(p00, 0,0); push(p11, 1,1); push(p01, 0,1);
            } else {
                // Water, or land whose tile is unavailable: flat-shaded colour.
                float r, g, b;
                if (water) {
                    r = opts.water_rgb[0] / 255.0f;
                    g = opts.water_rgb[1] / 255.0f;
                    b = opts.water_rgb[2] / 255.0f;
                } else {  // neutral land stand-in
                    r = 0.36f; g = 0.42f; b = 0.24f;
                }
                r *= shade; g *= shade; b *= shade;
                auto& vs = scene.flat.vertices;
                auto push = [&](const float* p) {
                    vs.push_back({p[0], p[1], p[2], r, g, b, 0.0f, 0.0f});
                };
                push(p00); push(p10); push(p11);
                push(p00); push(p11); push(p01);
            }
        }
    }

    if (min_h > max_h) { min_h = max_h = 0.0f; }
    scene.center[0] = 0.0f;
    scene.center[1] = (min_h + max_h) * 0.5f;
    scene.center[2] = 0.0f;
    scene.span = std::max({W * opts.xz_scale, H * opts.xz_scale, max_h - min_h, 1.0f});
    return scene;
}

void FillDefaultTerrainBand(fx::Palette& pal) {
    auto put = [&](int i, int r, int g, int b) {
        pal.r[i] = (std::uint8_t)r; pal.g[i] = (std::uint8_t)g; pal.b[i] = (std::uint8_t)b;
    };
    auto ramp = [&](int base, int n, int r0,int g0,int b0, int r1,int g1,int b1) {
        for (int k = 0; k < n; ++k) {
            float t = n > 1 ? (float)k / (n - 1) : 0.0f;
            put(base + k, (int)(r0 + (r1-r0)*t), (int)(g0 + (g1-g0)*t),
                          (int)(b0 + (b1-b0)*t));
        }
    };
    ramp(192, 16,  36,84,28,   84,140,64);   // vegetation green
    ramp(208, 16, 104,96,44,  132,116,72);   // dry grass / savanna
    ramp(224, 16,  96,76,52,  120,108,96);   // bare earth -> rock grey
    ramp(240, 15, 140,124,88, 196,180,140);  // highland / sand
    put(255, 252, 252, 252);                 // spec / snow white
}

}  // namespace fxg
