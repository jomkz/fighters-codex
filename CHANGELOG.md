# Changelog

All notable changes to this project will be documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versioning follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.3.0] - 2026-07-02

### Changed
- **BREAKING** Project renamed from fighters-toolkit to fighters-codex; binaries renamed `ft.exe` / `ft-gui.exe` → `fx.exe` / `fx-gui.exe` (#34)
- Project purpose restated: the RE documentation is the primary output; the tools are the validation layer (#33)
- Added the roadmap to 1.0 (`docs/roadmap.md`) — 7 constraint-gated phases, 18 epics — and made README/cli.md claims match implementation reality (#152)
- Release tooling ported from PowerShell to portable Python (#151)
- Documentation reorganized; conventional-commit and branch-naming conventions adopted (#16, #31, #32)

### Added
- Complete Ghidra RE pipeline for FA.EXE and all overlay DLLs — 14 portable, headless-ready scripts (#18, #20, #21)
- AI→BI compiler — all 9 stock flight-AI scripts compile to valid BI bytecode (#24); FA.EXE AI interpreter traced end-to-end (#27)
- 91 `FUN_`/`DAT_` placeholders resolved to real FA symbol names (#23)
- CN_INFO modem phone book mapped via differential saves, closing the NET.md gap (#28)
- **LAY**, **FNT**, **MUS**, **INF**, **HUD**, and **PAL** parsers with CLI commands (#15)
- `fx lib extract` — selective named-file extraction from LIB archives (#17)
- **SSF**, **PTS**, **DLG**, **PIC**, **T2**, **OT**, and **NT** format documentation from binary analysis (#11); DLG record sizes, LAY structure, ECM effectiveness bytes, JT agility/Pk fields, object flags, and MUS playlists confirmed (#8, #9, #10)
- Catch2 test scaffolding with passing ealib tests and VS Code tasks (#25)
- CB8 display fixes, improved VDO viewer, and Windows dark/light GUI theming (#30)
- `CLAUDE.md`, `SECURITY.md`, and epic/RE-task issue forms

### Fixed
- `audio_rate_from_ext` now maps `.22K` to 22050 Hz (#35)
- CLI usage text renamed from `ft` to `fx`, completing the project rename (#153)
- Mojibake em/en dash artifacts in the docs replaced with correct Unicode characters (#37)

## [0.2.0] - 2026-05-16

### Added
- Add download info and fix label sync action
- Add support for new formats to lib and CLI
- Feat/vscode build tasks
- Add DLL analyzer tool and documented FA overlay DLL format structures

### Fixed
- Docs/format inventory
- Update DLL format and ARCHITECTURE docs

## [0.1.0] - 2026-05-16

### Added
- Initial release of fighters-codex
- `fx_lib` — C++17 static library for reading and writing Jane's Fighters Anthology asset formats (LIB, PIC, SEQ, BRF/OT/NT, audio, mission, SH, CB8, RAW)
- `fx` — command-line tool for unpacking, inspecting, and repacking FA assets
- `fx-gui` — ImGui/DirectX 11 GUI editor for FA LIB archives with three-panel layout

[Unreleased]: https://github.com/jomkz/fighters-codex/compare/v0.3.0...HEAD
[0.3.0]: https://github.com/jomkz/fighters-codex/releases/tag/v0.3.0
[0.2.0]: https://github.com/jomkz/fighters-codex/releases/tag/v0.2.0
[0.1.0]: https://github.com/jomkz/fighters-codex/releases/tag/v0.1.0
