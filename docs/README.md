# Documentation

The primary output of this project is the reverse-engineering documentation: format specifications, engine architecture notes, and modding guides for Jane's Fighters Anthology and related titles, built up from binary analysis of the game's assets and executable. The toolkit software exists to exercise and validate that documentation — a working codec is the proof of understanding.

These pages are published as a searchable site at <https://jomkz.github.io/fighters-codex/>, built from this same `docs/` tree.

This is a work in progress and will be updated as more is discovered. If something here is wrong or needs more detail, please submit an issue.

## Fighters Anthology

Reverse-engineering notes, format specifications, and modding guides for Jane's Fighters Anthology (1998).

| Document | Description |
|----------|-------------|
| [fa/start-here.md](fa/start-here.md) | **Start here** — the learning path from game concepts to internals, for players and modders |
| [fa/README.md](fa/README.md) | FA knowledge base index |
| [fa/architecture.md](fa/architecture.md) | Runtime environment, asset system, and subsystem architecture |
| [fa/modding.md](fa/modding.md) | Step-by-step modding recipes |
| [fa/formats/README.md](fa/formats/README.md) | All 44 file format specifications, categorized |
| [fa/formats/STATUS.md](fa/formats/STATUS.md) | Generated per-format status matrix — spec completeness, codec direction, tests, fuzzing (CI-enforced) |
| [fa/reconstruction.md](fa/reconstruction.md) | Generated FA.EXE reconstruction matrix — per-subsystem naming + documentation progress (CI-enforced) |

Outstanding research is tracked on the [roadmap](roadmap.md) and the
[issue tracker](https://github.com/jomkz/fighters-codex/issues) — there is no
standalone TODO file.

## Toolkit

Reference documentation for the validation tools.

| Document | Description |
|----------|-------------|
| [roadmap.md](roadmap.md) | Phased roadmap to 1.0 — gates, epics, and the 1.0 definition |
| [cli.md](cli.md) | Full CLI command reference |
| [gui.md](gui.md) | fxs graphical editor feature reference |
| [api.md](api.md) | C++ library API reference |
| [development.md](development.md) | Building, IDE setup, and project structure |
| [spec-authoring.md](spec-authoring.md) | Format-spec template, front-matter schema, and vocabularies (CI-enforced) |
| [adr/README.md](adr/README.md) | Architecture decision records for the toolkit |
