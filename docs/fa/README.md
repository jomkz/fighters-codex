# Fighters Anthology

Jane's Fighters Anthology (1998, Electronic Arts) is a combat flight simulator covering over 80 aircraft across multiple theaters. This directory contains reverse-engineering notes, format specifications, and modding guides built up from binary analysis of the game's assets and executable.

The RE effort has documented all 46 known binary and text formats and the game's runtime as a set of
named, diagrammed subsystems. Two reconstruction programs are complete: the **game executable** (all
20 subsystems named and documented, epic #209) and its **overlay binaries** (7 binaries — audio,
comms, and the MSAPI matchmaking client — epics #247 / #272). Per-format completeness and codec
direction are tracked in the CI-enforced [status matrix](formats/STATUS.md); per-subsystem naming/doc
progress in the [reconstruction matrix](reconstruction.md); remaining open questions as issues on the
[roadmap](../roadmap.md). The `fx_lib` codecs and `fx` CLI validate the format specs — a byte-identical
round-trip is the proof a format is understood; the `fxc` source port is the same kind of proof for
the executable itself.

## Contents

| Document | Description |
|----------|-------------|
| [formats/](formats/README.md) | Binary and text format specifications — all 46 formats, categorized |
| [formats/STATUS.md](formats/STATUS.md) | Generated per-format status matrix (spec, codec, tests, fuzzing) |
| [reconstruction.md](reconstruction.md) | Generated reconstruction matrix — per-subsystem naming/doc progress across all 7 binaries (epics #209 / #247) |
| [architecture.md](architecture.md) | Runtime environment, asset system, overlay architecture, and the subsystem map |
| [game-loop.md](game-loop.md) | Main loop, initialization, per-frame dispatch, frame timing, and shutdown |
| [structs.md](structs.md) | Recovered runtime struct reference with per-field confidence |
| [globals.md](globals.md) | Named game-executable global variables, organized by subsystem |
| [symbols.md](symbols.md) | All 3,829 `FA.SMS` symbols, organized by subsystem |
| [modding.md](modding.md) | Step-by-step modding recipes — textures, stats, missions, audio, and more |

**Game-executable subsystems** (each with recovered symbols, a struct/field map, and a theme-aware
SVG; full index in the [reconstruction matrix](reconstruction.md)):
[objects](objects.md) · [shape-selection](shape-selection.md) · [ai-interpreter](ai-interpreter.md) ·
[wingman](wingman.md) · [physics](physics.md) · [collision](collision.md) · [weapons](weapons.md) ·
[renderer](renderer.md) · [render-core](render-core.md) · [terrain](terrain.md) · [hud](hud.md) ·
[network](network.md) · [sound](sound.md) · [input](input.md) · [memory-resource](memory-resource.md) ·
[campaign](campaign.md) · [shell-ui](shell-ui.md) · [startup](startup.md) · [seq](seq.md) ·
[video-decode](video-decode.md) · [view](view.md).

**Overlay binaries & boundary:** [wail32](wail32.md) (audio) · [comms](comms.md) /
[comms-modem](comms-modem.md) / [comms-screen](comms-screen.md) / [comms-transfer](comms-transfer.md)
(serial/modem) · [ip-tool](ip-tool.md) (EA tech-support tool) ·
[external-imports](external-imports.md) (the MS / third-party redistributable boundary).
