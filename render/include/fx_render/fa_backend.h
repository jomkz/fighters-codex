// fx_render — the FA-faithful software backend behind the generic Renderer
// API (fx_render #290, sub-issue #334): Mesh/Camera draws executed on the fa
// pipeline — indexed surface, 16.16 spans, Sutherland–Hodgman/near-plane
// clipping, painter's-order occlusion, no depth buffer anywhere.
//
// Colour fidelity boundary: generic meshes carry float RGB, so vertex and
// texel colours quantize through fa::Palette::Nearest — a stand-in for the
// engine's shade-table index lookups (inferred approximation). The pixel
// model, span stepping, clipping, and occlusion are the faithful part
// (docs/fa/renderer.md + render-core.md).
//
// MIT, clean-room from this project's own docs; see NOTICE.
#pragma once

#include <memory>

#include "fx_render/fa.h"
#include "fx_render/render.h"

namespace fx_render {

// The FA-faithful software renderer over `palette` — context-free, CPU only
// (usable headless, like the generic software backend). backend() reports
// Backend::Software; targets present the indexed surface as RGBA8 through
// the palette on Read(); native_texture() is 0 (hosts upload for display).
std::unique_ptr<Renderer> MakeFaRenderer(fa::Palette palette);

}  // namespace fx_render
