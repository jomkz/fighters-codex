# Changelog

All notable changes to this project will be documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versioning follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.5.3] - 2026-07-03

### Added
- **re** **The FA.EXE reconstruction program (epic #209) is complete — all 18 engine subsystems are named and documented.** Building on the object/entity subsystem shipped in v0.5.2, this release lands the remaining 17: renderer & rasterizer (#211), flight model & stores (#212), HUD/cockpit (#213), the "Chuck Talk" AI interpreter (#216), weapons/projectiles/ECM (#215), collision (#222), sound/music (#220), wingman/group AI (#217), video decode / Cobra (#227), memory & resource managers (#223), terrain (#221), 3D render core & SH interpreter (#228), campaign/mission/pilot (#218), network/multiplayer (#219), input (#224), shell/menu/dialog UI (#225), and startup/CRT (#226). Every code-referenced function in each subsystem's ranges is named (1,659 in scope) and every referenced global is named or waived; each subsystem has a `docs/fa/` page with a DB-checked symbol table and a theme-aware SVG flow diagram, tracked in the generated [reconstruction matrix](https://github.com/jomkz/fighters-codex/blob/main/docs/fa/reconstruction.md)
- **re** Reproducibility audit harness (`scripts/ghidra/rebuild_audit.sh` + `rebuild_diff.py`): rebuilds the Ghidra project from scratch (FA.EXE + FA.SMS + `db/symbols`) on a clean project and diffs the exported inventory against the committed one — turning "is this rebuildable from `db/`?" into a checked artifact. Verified 0 name drift across the program

### Changed
- **build** `ApplySymbols.java` hardened to survive symbol collisions (a name matching an existing FA.SMS label no longer aborts the run); `ExportInventory.java` takes an optional output dir; `check_status.py` coverage now recognises DB-wide global coverage (a shared struct interior is documented once); `DumpAllFunctions` decompiles in parallel and the headless heap default is raised to 8G

### Notes
- **Documentation + tooling release.** `fx_lib` and the `fx` CLI are byte-identical to v0.5.2 — no fa-bridge submodule bump. This closes milestone #8 (FA.EXE — Complete Reconstruction): the executable is fully named in the Ghidra project and documented subsystem-by-subsystem, rebuildable from the committed symbol database.

## [0.5.2] - 2026-07-03

### Added
- **re** FA.EXE reconstruction program (epic #209): a machine-readable symbol database under `db/` — a manifest of the 18 engine subsystems, per-subsystem VA→name CSVs (`db/symbols/`), and committed Ghidra ground-truth inventory exports — applied to the Ghidra project by `scripts/ghidra/ApplySymbols.java` and re-exported by `ExportInventory.java`. `tools/check_status.py` gains a reconstruction layer that enforces, per completed subsystem, that every code-referenced function is named and every referenced global is named or explicitly waived, cross-checks each subsystem doc's symbol table against the database, and generates the `docs/fa/reconstruction.md` progress matrix — all with self-test fixtures (#231)
- **re** Object/entity subsystem named and documented (`docs/fa/objects.md` + a theme-aware lifecycle diagram): the per-frame service chain, the `_cg`/`_cgt` current-object mirror, proc dispatch, arena allocation, and the remote hit/effect queues — 80/80 in-range functions named, referenced globals resolved (#210)
- **re** Shape-selection / whole-model damage swap documented (`docs/fa/shape-selection.md` + diagram): how `_SetupOT` derives the `_A`…`_D` variant set and how the engine swaps a destroyed object's model — the definitive answer to the A-10 `_A/_B/_C/_D` question (#214)
- **fx-gui** headless `--render <LIB> <ENTRY>` snapshot to PNG for automated visual review of the SH/PIC/editor render paths (#208)

### Changed
- **re** shape-selection `damage_set` (`+0x33`) resolved: written `_Rand(2)+1` by `PLANEBreakUp` at destruction, so a wreck's `{_A,_B}` vs `{_C,_D}` model pair is chosen at random per kill rather than fixed per aircraft (#210)
- **build** Ghidra whole-image decompile (`DumpAllFunctions`) parallelized across all cores via `ParallelDecompiler`; headless JVM heap default raised from 2G to 8G (#231)

### Fixed
- **fx-gui** correct mirrored SH 3D preview (#207)

### Notes
- **Documentation + tooling release.** `fx_lib` and the `fx` CLI are byte-identical to v0.5.1 — no fa-bridge submodule bump. `fx-gui` gains the headless `--render` snapshot (#208) and the mirrored-preview fix (#207). The release stands up epic #209 (complete FA.EXE reconstruction) with its first subsystem (#210) done and the machinery every subsequent subsystem builds on: the `db/` symbol database, the apply/export Ghidra scripts, CI coverage enforcement, and the reconstruction matrix. Remaining subsystems are tracked as sub-issues #211–#228.

## [0.5.1] - 2026-07-03

### Changed
- **re** SH header: the two unknown header words are named and traced — `radius` (approximate bounding-sphere magnitude, read by `GRAddBrentObj` to floor the projection/precision shift) and `radius_world` (shown engine-unused, present only on ground/naval scenery), replacing the incorrect "file ID" guess (#124)
- **re** SH opcode table cross-validated against the OpenFA `sh` crate (GPLv3): full agreement on all 55 opcodes, sizes, and formulas; two inert modeling differences recorded; mnemonic provenance attributed per the MIT/GPL boundary (#121)
- **re** SH interpreter dispatch recovered: the hand-written threaded-code `vector_table` (128 handlers indexed by `opcode×2`) names a dozen former `Unk*` handlers and shows the byte/word-magic split is a parser-side model the engine does not have (#123)
- **re** SH animation and LOD/damage opcodes fully specified — `JumpToFrame` free-running frame selection against the global frame counter, and the `JumpToDamage`/`JumpToDetail`/`JumpToLOD` conditional geometry switches — enough to implement playback and LOD/damage-state selection from the doc (#122, #123)
- **re** SH X86Unknown region specified: the embedded-x86 blocks are trampoline-based conditional selectors (the `0xF0 → push esi; ret` entry, `FF25` reads of the `_PL*` articulation state, `do_start_interp` re-entry, and a per-shape inventory), making the fa-bridge x86-effect interpreter implementable from the doc (#125)

### Notes
- **v0.5.1 is a documentation release** completing epic #52 (SH engine-behavior semantics), pulled forward ahead of Phase 4. It carries no runtime-code changes — `fx_lib`, `fx`, and `fx-gui` binaries are byte-identical to v0.5.0, and no fa-bridge submodule bump is required. With it, the SH format spec is sufficient for the fa-bridge bytecode (#19) and x86-effect (#21) interpreters to be built from documentation alone. The Linux Ghidra workbench port that enabled this work landed in the same window (#199).

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

[Unreleased]: https://github.com/jomkz/fighters-codex/compare/v0.5.3...HEAD
[0.5.3]: https://github.com/jomkz/fighters-codex/releases/tag/v0.5.3
[0.5.2]: https://github.com/jomkz/fighters-codex/releases/tag/v0.5.2
[0.5.1]: https://github.com/jomkz/fighters-codex/releases/tag/v0.5.1
[0.5.0]: https://github.com/jomkz/fighters-codex/releases/tag/v0.5.0
[0.4.3]: https://github.com/jomkz/fighters-codex/releases/tag/v0.4.3
[0.4.2]: https://github.com/jomkz/fighters-codex/releases/tag/v0.4.2
[0.4.1]: https://github.com/jomkz/fighters-codex/releases/tag/v0.4.1
[0.4.0]: https://github.com/jomkz/fighters-codex/releases/tag/v0.4.0
[0.3.0]: https://github.com/jomkz/fighters-codex/releases/tag/v0.3.0
[0.2.0]: https://github.com/jomkz/fighters-codex/releases/tag/v0.2.0
[0.1.0]: https://github.com/jomkz/fighters-codex/releases/tag/v0.1.0
