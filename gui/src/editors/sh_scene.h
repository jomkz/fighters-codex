#pragma once
// Display-free SH scene core (#366). Turns a parsed SH mesh into fx_render
// triangle lists — the pre-shaded palette colours, decal semantics and Z-up
// remap the preview panel established (docs/fa/render-core.md) — and renders
// standalone thumbnails through the FA-faithful software backend, which is
// context-free (no GPU, no window). No ImGui, no SDL: the preview panel and
// the thumbnail worker share this one geometry path, and gui_tests drive it
// headless.
#include "fx/pal.h"
#include "fx/sh.h"
#include "fx_render/fa.h"
#include "fx_render/render.h"
#include <memory>

namespace fxg {

// Triangulate the SH mesh into two fx_render vertex lists: textured faces
// (normalized u,v against tex_w/tex_h, flipped V for top-left decoded PICs)
// and flat-shaded faces. Face colours are FA's pre-shaded palette-ramp
// entries applied directly — no runtime relighting — and ride along on
// textured vertices as the transparent-texel fallback. Pass tex_w/tex_h = 0
// when no texture is available; every face then lands in `flat`.
void BuildShMeshes(const fx::ShMesh& mesh, const fx::Palette& pal,
                   int tex_w, int tex_h,
                   fx_render::Mesh& flat, fx_render::Mesh& textured);

// The preview palette as the fa backend's 192-entry 6-bit palette:
// fx::Palette stores VGA components widened to 8-bit, so >>2 recovers them.
fx_render::fa::Palette ToFaPalette(const fx::Palette& pal);

// Render one SH record to a square RGBA thumbnail through the fa software
// backend: parse (merged default state), build the meshes, frame the model
// with the preview's default three-quarter orbit, draw, read back. `texture`
// is the shape's decoded skin PIC or null (faces then flat-shade). Returns an
// empty Image (width 0) when the record has no renderable geometry —
// x86-only shapes and parse failures.
fx_render::Image RenderShThumbnail(const uint8_t* sh, size_t size,
                                   const fx::Palette& pal,
                                   std::shared_ptr<const fx_render::Image> texture,
                                   int px);

}  // namespace fxg
