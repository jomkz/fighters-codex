# Fighters Anthology

Jane's Fighters Anthology (1998, Electronic Arts) is a combat flight simulator covering over 80 aircraft across multiple theaters. This directory contains reverse-engineering notes, format specifications, and modding guides built up from binary analysis of the game's assets and executable.

The RE effort has covered all 44 known binary and text formats, the full FA.EXE symbol map (3,829 recovered C++ symbols), and the major runtime subsystems — flight model, renderer, AI, networking, and the Win32 overlay DLL architecture. The `ft_lib` codecs and `ft` CLI commands exist to validate the format specs: a working, byte-identical round-trip implementation is the proof that a format is fully understood. Cross-references between the docs and the tools are noted throughout.

## Contents

| Document | Description |
|----------|-------------|
| [architecture.md](architecture.md) | Runtime environment, asset system, overlay architecture, and all major subsystems |
| [modding.md](modding.md) | Step-by-step modding recipes — textures, stats, missions, audio, and more |
| [todo.md](todo.md) | Outstanding research items and implementation backlog |
| [formats/](formats/README.md) | Binary and text format specifications (44 formats documented) |
