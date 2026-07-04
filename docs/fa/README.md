# Fighters Anthology

Jane's Fighters Anthology (1998, Electronic Arts) is a combat flight simulator covering over 80 aircraft across multiple theaters. This directory contains reverse-engineering notes, format specifications, and modding guides built up from binary analysis of the game's assets and executable.

The RE effort has documented all 44 known binary and text formats, the full FA.EXE symbol map (3,829 recovered C++ symbols), and the major runtime subsystems — flight model, renderer, AI, networking, and the Win32 overlay DLL architecture. Per-format completeness, codec direction, and test coverage are tracked in the CI-enforced [status matrix](formats/STATUS.md); remaining open questions are tracked as issues on the [roadmap](../roadmap.md). The `fx_lib` codecs and `fx` CLI commands exist to validate the format specs: a working, byte-identical round-trip implementation is the proof that a format is fully understood. Cross-references between the docs and the tools are noted throughout.

## Contents

| Document | Description |
|----------|-------------|
| [formats/](formats/README.md) | Binary and text format specifications — all 44 formats, categorized |
| [formats/STATUS.md](formats/STATUS.md) | Generated per-format status matrix (spec, codec, tests, fuzzing) |
| [reconstruction.md](reconstruction.md) | Generated FA.EXE reconstruction matrix — per-subsystem naming/doc progress (epic #209) |
| [architecture.md](architecture.md) | Runtime environment, asset system, overlay architecture, and all major subsystems |
| [game-loop.md](game-loop.md) | Main loop, initialization, per-frame dispatch, frame timing, and shutdown |
| [objects.md](objects.md) | Object/entity system — the service chain, current-object mirror, and proc dispatch |
| [shape-selection.md](shape-selection.md) | Whole-model damage swap and the per-class `_A`…`_D` variant set |
| [globals.md](globals.md) | Named FA.EXE global variables, organized by subsystem |
| [network.md](network.md) | Multiplayer networking internals — transports, session discovery, frame sync |
| [physics.md](physics.md) | Physics, flight model, and collision detection |
| [renderer.md](renderer.md) | 3D rendering pipeline — shape interpreter, rasterizer, terrain, atmosphere |
| [structs.md](structs.md) | Recovered runtime struct reference with per-field confidence |
| [symbols.md](symbols.md) | All 3,829 FA.SMS symbols, organized by subsystem |
| [modding.md](modding.md) | Step-by-step modding recipes — textures, stats, missions, audio, and more |
