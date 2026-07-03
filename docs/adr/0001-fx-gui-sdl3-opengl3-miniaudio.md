# ADR-0001: fx-gui cross-platform backend — SDL3 + OpenGL 3.3 + miniaudio

**Status:** Accepted (2026-07-02)
**Serves:** [epic #46](https://github.com/jomkz/fighters-codex/issues/46)
([#85](https://github.com/jomkz/fighters-codex/issues/85) is this record); informs
[fighters-legacy#156](https://github.com/fighters-legacy/fighters-legacy/issues/156)

## Context

`fx-gui` is the interactive validation surface for the format research, but it is
Windows-only: a Win32 message pump and DX11 swapchain host
([gui/src/main.cpp](https://github.com/jomkz/fighters-codex/blob/main/gui/src/main.cpp)),
`waveOut` audio preview, Win32 common dialogs, and registry-based dark/light theming.
Primary development moved to Fedora (see [development.md](../development.md)), so the
GUI cannot be exercised against the local FA install where the rest of the toolkit
lives. [Phase 3 of the roadmap](../roadmap.md) requires parity on Fedora and Windows,
shipping in CI and releases.

Constraints that shaped the decision:

- **Dependency ethos.** The toolkit builds without a package manager; runtime
  dependencies are vendored (Dear ImGui, stb) and the only network fetch is Catch2
  for tests. A windowing library is the first dependency too large to vendor as
  source.
- **One small 3D preview.** The only bespoke GPU work is the SH mesh viewer — an
  offscreen render of a few hundred triangles. Whatever the backend, Dear ImGui does
  the rest.
- **Lesson transfer.** The fighters-legacy engine's platform layer is SDL3-based with
  pure-virtual HAL interfaces; its upcoming `IGui` HAL
  ([fighters-legacy#156](https://github.com/fighters-legacy/fighters-legacy/issues/156))
  will host Dear ImGui behind `imgui_impl_sdl3`. The closer this port's seams are to
  that design, the more the porting experience feeds it.

## Options considered

### Windowing + rendering

| Option | Assessment |
|--------|------------|
| **SDL3 + OpenGL 3.3 core** (chosen) | Both ImGui backends (`imgui_impl_sdl3`, `imgui_impl_opengl3`) are mature, maintained, and version-locked to the vendored ImGui. GL 3.3 core is everywhere fx-gui needs to run, including Mesa llvmpipe for headless CI. SDL3 additionally covers file dialogs, system-theme queries and change events, display scale, and clipboard — eliminating three would-be dependencies. Matches fighters-legacy's platform layer. |
| SDL3 + SDL_GPU | Wrong altitude for this tool: adds a shader-toolchain step (SDL_shadercross/offline compile) and a younger ImGui backend for zero visual benefit on a 200-triangle preview. Revisit if fx-gui ever needs real rendering throughput. |
| SDL3 + Vulkan | Maximum boilerplate for the same preview; complicates headless CI (llvmpipe GL is simpler than lavapipe swapchains); `imgui_impl_vulkan` integration effort belongs in fighters-legacy where Vulkan is the actual renderer. |
| GLFW + OpenGL 3.3 | Lighter than SDL3 but covers only windowing/input — dialogs, theme detection, and DPI-change events would each need another dependency or per-OS code, and nothing transfers to fighters-legacy. |

### Audio preview

| Option | Assessment |
|--------|------------|
| **miniaudio** (chosen) | Single vendored header (public domain / MIT-0), fits the stb-style convention. Loads platform audio backends (WASAPI, ALSA/PulseAudio/PipeWire) at runtime via `dlopen`, so it adds zero link-time dependencies and keeps artifacts self-contained. Keeps audio decoupled from windowing — mirroring fighters-legacy's separate `IAudio` HAL. Its null backend enables hardware-free unit tests of the player state machine. |
| SDL3 audio | No extra dependency, but couples audio preview to the windowing library and diverges from fighters-legacy's audio split. |
| OpenAL Soft | A 3D-audio API for one-shot mono PCM preview is overkill, and it is an external library to acquire on both OSes. |

### SDL3 acquisition

| Option | Assessment |
|--------|------------|
| **System-first + pinned fallback** (chosen) | `find_package(SDL3 CONFIG)` first — Fedora ships `SDL3-devel`, so local builds use the system package with zero configure-time network. Where absent, FetchContent builds a **pinned release tarball (URL + SHA-256), static**. `FX_SDL3_VENDORED=ON` forces the pinned path; CI and release jobs set it so shipped artifacts never silently pick up a system SDL3. Both acquisition paths get exercised for free (Fedora developers vs. CI runners). |
| FetchContent always | Reproducible everywhere but recompiles SDL3 on every local first configure and ignores the system package the primary platform already provides. |
| Vendor SDL3 source / require system everywhere | SDL3 is far too large to vendor in-tree; system-only is impossible on GitHub's Ubuntu runners (no `libsdl3-dev` ≤ 24.04) and would make Linux artifacts depend on `libSDL3.so`. |

## Decision

`fx-gui` runs on **SDL3** (window, GL context, input, file dialogs, theme, DPI) with
**OpenGL 3.3 core** rendering through the stock ImGui backends, a vendored **glad**
GL loader for first-party GL code, and **miniaudio** for audio preview. The SH
preview is reimplemented as a GL FBO pipeline with GLSL ports of the HLSL shaders.
Initial pin: **SDL 3.4.12** (hash recorded alongside the FetchContent declaration in
[gui/CMakeLists.txt](https://github.com/jomkz/fighters-codex/blob/main/gui/CMakeLists.txt)).
The DX11/Win32 path is removed, not maintained in parallel — Windows uses the same
SDL3/GL3 backend.

## Consequences

- **Dependency policy amendment.** The "only network fetch is Catch2" rule becomes
  "Catch2, plus SDL3 when no system package exists" — system-first, pinned, and
  checksummed. miniaudio and glad are vendored under the existing policy.
  [development.md](../development.md) documents the amended rule; this ADR is the
  authority for why.
- **CI.** Ubuntu runners install X11/Wayland dev headers so FetchContent can compile
  SDL3; the GUI enters CodeQL scope on Linux; a headless smoke test runs under
  Xvfb/llvmpipe. GitHub's Windows runners expose no GL 3.3, so Windows CI
  compile-checks the GUI and runs its display-free unit tests only.
- **Artifacts stay self-contained.** Static SDL3 in release builds (SDL resolves
  platform libraries at runtime via `dlopen`), so the Linux tarball and Windows zip
  remain single executables.
- **Wayland caveat.** Window *position* save/restore is a no-op under Wayland
  (size and maximized state still restore). Accepted; documented in
  [gui.md](../gui.md).
- **Behavior changes.** Settings move from a CWD-relative `fx-gui.ini` to the
  per-user SDL preferences path; theming follows `SDL_GetSystemTheme` instead of the
  Windows registry; DPI scaling becomes dynamic.

## Lessons for fighters-legacy#156

Seeded here; finalized with findings from the port itself.

1. **Frame lifecycle.** `IGui::beginFrame` must run after the platform event pump;
   `endFrame` should emit into a render target it is handed (the frame's command
   buffer under Vulkan) — presentation belongs to `IRenderer`, never `IGui`.
2. **Texture handles.** Editors that only ever touched an opaque `ImTextureID`
   ported for free; the one leak of a backend type (`App` holding `ID3D11Device*`)
   is exactly what had to be rewritten. `IGui::drawImage` should take an opaque
   64-bit `TextureHandle` minted by `IRenderer`.
3. **Input arbitration.** `wantsKeyboardInput()/wantsMouseInput()` map to
   `io.WantCaptureKeyboard/Mouse` and are valid only after `beginFrame` — forward
   events to the GUI unconditionally and have the sim consult the flags per frame.
4. **DPI.** Style rebuild must be idempotent (ImGui's `ScaleAllSizes` is cumulative);
   `IGui` wants a `dpiChanged(scale)` notification from `IWindow`.
5. **Dialogs are async.** SDL3's file dialogs are callback-based and may complete on
   any thread — a blocking dialog API in a HAL would be unimplementable on the SDL3
   backend. The workable shape: continuation queue pumped on the main thread,
   capture-by-value, revalidate state on arrival.
6. **Audio.** A control-plane (main thread) / data-plane (realtime callback) split
   with atomics-only crossings ports cleanly between audio APIs; track playback
   position from the submission cursor, not device queries.
7. **Theme** is a platform concern (query + change event); `IGui` receives it rather
   than detecting it.
