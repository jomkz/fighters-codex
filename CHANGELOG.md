# Changelog

All notable changes to this project will be documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versioning follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.5.0] - 2026-07-03

### Added
- **fx-gui** INF styled editor: directive sections rendered with their in-game alignment and title/body weight, editable per section (text, alignment, style, insert/delete) alongside a raw-source tab. Underneath it, the INF codec is upgraded to a **byte-identical round-trip** — sections keep their exact source bytes, proven against all 269 tech sheets in FA_3.LIB — delivering the INF slice of #101 early (#93)

### Changed
- docs/gui.md is fully current with the ported, cross-platform GUI: every #47 feature documented in its own PR, the last stale planned items pruned, panel and loose-file claims matched to the code (#94)

### Notes
- **v0.5.0 marks the Phase 2 + Phase 3 roadmap gates** (#185): documentation system (spec template, CI-enforced status matrix, published site) and the fx-gui cross-platform port with the #47 validation feature set — palette viewer/switcher (v0.4.2), full-row SEQ editing (v0.4.3), and the INF editor above. P2/P3 content that shipped in v0.4.0–v0.4.3 is not re-announced here.

## [0.4.3] - 2026-07-03

### Added
- **fx-gui** full-row SEQ event editing: time (`N`/`+N`), command, sync, and args are all editable per row, completing the add/insert/delete buttons that shipped half-usable in #30. Edited rows are rebuilt tab-separated and re-parsed through `fx::seq_parse` (retiring the editor's quote-stripping inline tokenizer), insert inherits the row's addressing mode so relative `+` chains keep resolving, and append lands after the resolved timeline end rather than a plain max over ticks (#92)

## [0.4.2] - 2026-07-03

### Added
- **fx-gui** palette viewer and switcher: `.PAL` records (previously unopenable) show a 16×16 swatch grid with per-index RGB tooltips, and one shared palette selection — Auto, Greyscale, or any `.PAL` across open sessions — applies live to PIC previews and CB8 frames. Auto keeps the established defaults (PALETTE.PAL for PIC, greyscale for CB8, whose engine palette ships in no LIB); PIC → PNG and CB8 frame exports follow the selection, and the PIC editor shows its inline palette fragment (#91)

## [0.4.1] - 2026-07-03

### Changed
- All `fa-content` references updated to **fa-bridge**, following the plugin repo's rename — the roadmap, README, api.md embedding example, CLAUDE.md, two CMake comments, and the release script's post-release reminder (#161)

### Fixed
- The release flow no longer tags before the release PR merges — v0.4.0's tag initially pointed at a commit that could never land on protected `main`. `scripts/release.py` is branch-aware: it commits on `chore/release-vX.Y.Z` (created automatically when run from `main`, refused anywhere else), never tags, guards against leftover tags/branches and double changelog rotation, and prints the push → PR → squash-merge → retag steps now documented in development.md § Releasing (#188)

## [0.4.0] - 2026-07-03

### Added
- `fx_lib`, the `fx` CLI, and the full test suite build and run natively on Linux (GCC and Clang) alongside Windows, from the same tree — MSVC-isms replaced with portable seams, and CMake presets (`msvc`, `gcc`, `clang`, `asan-ubsan`, `release`) with a rewritten development.md covering both workflows (#65, #66, #67, #68, #69)
- `FX_FA_ROOT` integration mode: pointing the build at a real FA install registers the `fa_extract_manifest` test, which verifies every extracted byte against a committed SHA-256 manifest — extraction proven byte-identical across platforms; a CLI end-to-end round-trip test joins the suite
- `fx-gui` runs natively on Linux and Windows: SDL3 + OpenGL 3.3 host, native file dialogs, system-theme detection with live switching, DPI scaling, and a `--smoke` headless self-check; settings move to the per-user preferences path (#86, #88)
- Audio preview via miniaudio, replacing the Windows `waveOut` path (#87)
- ADR-0001 records the GUI backend selection (SDL3 + OpenGL 3.3 + miniaudio) and the SDL3 acquisition policy — system-first with a pinned, checksummed FetchContent fallback (#85)
- `fx::ealib_safe_name` joins the `fx_lib` API — portable sanitization of standalone-file entry names
- CI runs the test suite on every leg and gains Linux GCC/Clang legs, an ASan/UBSan job, CodeQL for C++, a coverage ratchet, and a libFuzzer scaffold with smoke run; all actions pinned by SHA (#70, #71, #72, #73, #74)
- Linux x64 release artifacts: `fx` and the `fx_lib` developer SDK ship as tar.gz alongside the Windows zips (glibc 2.35+, libstdc++ statically linked) (#75)
- Linux `fx-gui` tarball joins the release artifacts; CI builds and smoke-tests the GUI on both OSes, and the new `gui_tests` suite covers the dialog queue, preview math, and audio player state machine on every leg (#90)
- Format-spec template with front-matter schema, the `tools/check_status.py` checker, and a generated per-format status matrix, enforced in CI (#174)
- The docs tree is published as an mkdocs-material site at <https://jomkz.github.io/fighters-codex/>, with a strict build that fails on broken links, missing nav pages, or non-blob links out of the docs tree (#181)

### Changed
- All format specs restructured to the template in four batches; engine docs normalized with uniform provenance and shared vocabulary (#176, #177, #178, #179, #180)
- The SH 3D preview renders through a GL 3.3 FBO pipeline (GLSL port of the DX11 shaders); the Win32/DX11 backend and its vendored ImGui backends are removed (#86)

### Fixed
- LIB extraction rejects flags=4 entries whose decompressed-size prefix exceeds 64 MiB — a crafted archive could previously force multi-GiB allocations (#168)
- A zero decompressed-size claim in a flags=4 entry no longer triggers undefined behavior in the DCL decompressor (#169)
- The `fx` CLI packs LIB archives in deterministic order, handles paths portably, and writes CSV exports in binary mode so line endings are LF on every platform
- Glyph-sheet size math is widened before allocation in the FNT path
- `lib extract` is documented in the CLI usage text

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

[Unreleased]: https://github.com/jomkz/fighters-codex/compare/v0.5.0...HEAD
[0.5.0]: https://github.com/jomkz/fighters-codex/releases/tag/v0.5.0
[0.4.3]: https://github.com/jomkz/fighters-codex/releases/tag/v0.4.3
[0.4.2]: https://github.com/jomkz/fighters-codex/releases/tag/v0.4.2
[0.4.1]: https://github.com/jomkz/fighters-codex/releases/tag/v0.4.1
[0.4.0]: https://github.com/jomkz/fighters-codex/releases/tag/v0.4.0
[0.3.0]: https://github.com/jomkz/fighters-codex/releases/tag/v0.3.0
[0.2.0]: https://github.com/jomkz/fighters-codex/releases/tag/v0.2.0
[0.1.0]: https://github.com/jomkz/fighters-codex/releases/tag/v0.1.0
