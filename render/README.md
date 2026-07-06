# fx_render — shared renderer

`fx_render` is a small, backend-agnostic renderer with **one** geometry→pixels API and
interchangeable backends, so the OpenGL and software render paths are built **once** and shared
across the reconstruction's consumers instead of re-implemented three times:

- **fxs** — the asset viewer (its SH 3D preview renders through this module).
- **fxe** — the clean-room source port of the game executable.
- **fighters-legacy** — the GPL engine may adopt it for its classic/parity path
  ([fighters-legacy#669](https://github.com/fighters-legacy/fighters-legacy/issues/669)).

MIT, clean-room from this project's own documentation. Part of epic
[#281](https://github.com/jomkz/fighters-codex/issues/281).

## API

`#include <fx_render/render.h>` (link `fx::render`). A frame is `Begin` → one or more `Draw` →
`End`; targets are backend-owned offscreen surfaces:

| Type | Role |
|---|---|
| `Vertex` / `Mesh` | interleaved position + linear RGB + `u,v`; a triangle **or** line list. `Mesh::texture` (optional, nearest-sampled RGBA) textures a filled-triangle draw with the per-vertex `u,v` (0..1) instead of the colour — one texture per mesh (split multi-texture models caller-side) |
| `Camera` | column-major 4×4 MVP (OpenGL convention) |
| `Image` | owned RGBA8 CPU buffer (top-left origin) |
| `DrawOptions` | primitive (triangles/lines), depth test/write, wireframe overlay, cull |
| `RenderTarget` | offscreen surface; `Read` → CPU `Image`, `native_texture()` → GL id or 0 |
| `Renderer` | `MakeTarget` / `Begin` / `Draw` / `End`; `MakeRenderer(Backend)` |

```cpp
auto r = fx_render::MakeRenderer(fx_render::Backend::Software);
auto t = r->MakeTarget(256, 256);
r->Begin(*t, {0, 0, 0, 255});
r->Draw(mesh, cam, {});          // DrawOptions{} = filled triangles, depth on
r->End();
fx_render::Image img; t->Read(img);   // img.pixels is now RGBA8
```

`RenderToImage(...)` is a one-shot convenience for the single-mesh case (used by the tests).

## Backends

- **Software** — a context-free CPU rasteriser (barycentric Gouraud fill + shared depth buffer +
  line/wireframe). No GPU, no window: the only backend usable in headless tests and `fxe`
  validation, and the substrate for the FA-faithful `GG_/G_` pipeline (#290). Always available.
- **OpenGL** — the GPU path (`#include <fx_render/gl.h>`, `MakeOpenGLRenderer()`), extracted from
  fxs's preview. Targets are FBOs; `native_texture()` is the colour attachment for zero-copy
  ImGui display. Requires a current GL 3.3 core context with glad loaded (the host provides both).

## FA-faithful path (`fx_render::fa`)

`#include <fx_render/fa.h>` — the faithful reproduction of the game's `GG_/G_` pixel pipeline
([renderer.md](../docs/fa/renderer.md) + [render-core.md](../docs/fa/render-core.md), issue
[#290](https://github.com/jomkz/fighters-codex/issues/290)), growing per sub-issue:

| Type | Role |
|---|---|
| `fa::Surface` | 8-bit indexed target with the documented bitmap record's semantics — runtime width/height/stride + row-pointer table |
| `fa::Palette` | 192-entry 6-bit VGA palette; `Present` → RGBA8 `Image`, `Nearest` → index remap (`G_RemapBitmapToPalette`) |
| `fa::Raster` | the `G_*` state block — clip box (`G_Init`/`G_SetClipBox`), `_cColor`, `_cFillType` — plus `Point`/`Rect` |
| `fa::Fx` | 16.16 fixed-point screen coordinates (`ToFx` / `FxFloor`) |

Dimensions are runtime parameters: FA.EXE's 1024×768 ceiling is a `GG_`/DirectDraw *device*
limit the fa path does not inherit (resolution-independence guards in `tests/render/`).

Sub-issue progress: **#328** surface/palette/state ✅ · **#329** 16.16 YLR spans ✅ ·
**#330** Gouraud ✅ · #331 clipping · #332 painter's order · #333 textured spans · #334 fxs
software mode + fidelity sweep.

## Roadmap (sub-issues of #281)

1. **#288** — module: API + software-backend foundation ✅
2. **#289** — OpenGL backend, extracted from `gui/src/panels/preview.cpp` ✅
3. **#291** — fxs refactored onto `fx::render` (SH preview) ✅
4. **#290** — FA-faithful software rasteriser — in progress, decomposed into #328–#334 (section above)
5. **#292** — fxe runtime render hook — re-homed to the fxe milestone (#280)
