#pragma once
// Display-free T2 terrain preview core (#285). Builds a 3D heightfield mesh
// from a .T2 leaf grid and renders it through the shared fx_render module —
// the same geometry->pixels path the SH model preview uses — so the terrain
// draws the way it would in game (docs/fa/formats/T2.md § Terrain Texturing).
// No ImGui, no SDL, so gui_tests drives it headless.
//
// Each leaf's texture_variant (0..31) selects a theater texture tile
// (<name><N>.PIC); the caller supplies the decoded tiles. Water leaves
// (surface class 0xFF) are drawn as a flat sea colour rather than textured.
#include "fx/pal.h"
#include "fx/t2.h"
#include "fx_render/render.h"
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace fxg {

// Supplies the decoded RGBA tile for a texture variant, or null when that
// tile is unavailable (the leaf then falls back to a flat land colour).
using TileProvider =
    std::function<std::shared_ptr<const fx_render::Image>(std::uint8_t variant)>;

struct TerrainScene {
    // One textured mesh per texture variant that resolved to a tile, plus a
    // single flat-shaded mesh for water and tile-less land.
    std::vector<fx_render::Mesh> textured;
    fx_render::Mesh flat;

    float center[3] = {0.0f, 0.0f, 0.0f};  // bbox centre, render Y-up space
    float span = 1.0f;                     // largest extent, for camera framing
    int water_leaves = 0;                  // diagnostics for tests/UI
    int land_leaves = 0;
    int textured_leaves = 0;
};

struct TerrainBuildOptions {
    float xz_scale = 1.0f;       // world size of one leaf cell
    float height_scale = 3.0f;   // world Y per elevation band
    bool  shade = true;          // apply a fixed directional light for relief
    std::array<std::uint8_t, 3> water_rgb{{40, 72, 120}};  // flat sea colour
};

// Build the terrain scene from a decoded T2 map. Corner heights are the
// average of the leaves meeting at each grid vertex, so the heightfield is
// continuous. Returns an empty scene (no meshes) when the map has no leaves.
TerrainScene BuildTerrainScene(const fx::T2Map& map, const TileProvider& tiles,
                               const TerrainBuildOptions& opts = {});

// Fill palette indices 192..255 — the terrain band the theater atmosphere
// fills at scene init (docs/fa/formats/T2.md § Terrain Texturing) — with a
// default earthy ramp, a stand-in for the atmosphere/LAY-driven band. Entries
// 0..191 are left untouched.
void FillDefaultTerrainBand(fx::Palette& pal);

}  // namespace fxg
