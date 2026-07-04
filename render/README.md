# fx_render — shared renderer

`fx_render` is a small, backend-agnostic renderer with **one** geometry→pixels API and
interchangeable backends, so the OpenGL and software render paths are built **once** and shared
across the reconstruction's consumers instead of re-implemented three times:

- **fx-gui** — the asset viewer (source of the OpenGL backend).
- **fxc** — the clean-room source port of the game executable.
- **fighters-legacy** — the GPL engine may adopt it for its classic/parity path
  ([fighters-legacy#669](https://github.com/fighters-legacy/fighters-legacy/issues/669)).

MIT, clean-room from this project's own documentation. Part of epic
[#281](https://github.com/jomkz/fighters-codex/issues/281).

## API

`#include <fx_render/render.h>` (link `fx::render`):

| Type | Role |
|---|---|
| `Vertex` | object-space position + linear RGB (grows to normals/UVs) |
| `Mesh` | non-indexed triangle list (3 vertices per triangle) |
| `Camera` | column-major 4×4 MVP (OpenGL convention) |
| `Image` | owned RGBA8 offscreen target (row-major, top-left origin) |
| `RenderOptions` | wireframe, backface cull, clear colour |
| `Renderer` / `MakeRenderer(Backend)` | draw meshes into an `Image` |

```cpp
auto r = fx_render::MakeRenderer(fx_render::Backend::Software);
fx_render::Image img; img.resize(256, 256);
r->Render(mesh, cam, img, {});   // img.pixels is now RGBA8
```

## Backends

- **Software** — a context-free CPU rasteriser (barycentric Gouraud fill + depth buffer). No GPU,
  no window: the only backend usable in headless tests and `fxc` validation, and the substrate for
  the FA-faithful `GG_/G_` pipeline. Always available.
- **OpenGL** — the GPU path matching the game's hardware rendering; requires a current GL context.
  `MakeRenderer(OpenGL)` returns `nullptr` until it is wired.

## Roadmap (sub-issues of #281)

1. **#288** — this module: API + software-backend foundation ✅
2. **#289** — OpenGL backend, extracted from `gui/src/panels/preview.cpp`
3. **#290** — FA-faithful software rasteriser (painter's sort, fixed-point spans, flat/Gouraud)
4. **#291** — refactor fx-gui onto `fx::render`
5. **#292** — fxc runtime render hook
