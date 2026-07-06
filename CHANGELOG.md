# Changelog

All notable changes to this project will be documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versioning follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

Checkpoint release for Phase 4 (Codec & Test Completeness), Wave 3 — **the
image and cockpit-UI formats round-trip**: PIC, CB8, RAW, and FNT gain
byte-identical repack, HUD and LAY gain write paths and an in-game-style
preview, and the whole image/A-V codec surface is hardened by a new fuzz
batch that found and fixed five memory-safety bugs on contact. Alongside
Wave 3, the `fx_render` **software rasterizer** (the `fa` backend) is
complete, and the documentation site is rebuilt as a learning experience.

### Added
- **fx-lib** **Byte-identical repack for the image formats.** `PIC`
  (dense + sparse, proven over a full-install census, #175), `CB8` (the
  FMV container — an engine-traced VQ-keyframe model with per-frame
  palette, #95), `RAW` (screenshot import + repack, header confirmed at
  four resolutions, #96), and `FNT` (an x86 glyph recompiler that
  reproduces every install font byte-for-byte, #97).
- **fx-lib** **HUD and LAY write paths (#99).** `hud_repack` / `lay_repack`
  rebuild the cockpit-overlay DLLs around edited gauge parameters, icon
  labels, and atmosphere layers, byte-identical over all 46 HUD and 24 LAY
  files; exposed as `fx hud set` and `fx lay set`.
- **fxs** **HUD and LAY in-game-style previews (#283).** Both editors draw
  a true draw-semantics preview through `fx_render` — HUD symbology
  positioned from the file's gauge parameters, LAY sky rendered as the
  engine's Gouraud horizon banding.
- **fx-render** **The `fa` software rasterizer (#328–#334).** A faithful
  reimplementation of the game's `G_*` raster layer behind the generic
  Renderer API: 8-bit indexed surface + VGA palette + raster state, a
  16.16 fixed-point span core, Gouraud (packed-index) spans,
  Sutherland–Hodgman / Cohen–Sutherland / near-plane clipping,
  painter's-order occlusion (no z-buffer), and affine + perspective
  textured spans — with an fxs **Software (FA)** rendering mode.
- **test** Codec suites: `cb8`/`ot`/`fnt` error paths and a synthetic FNT
  round-trip (#112); `hud`/`inf`/`lay`/`raw`/`sms` suites, including the
  `FA.SMS` symbol-map parser cross-checked against the reconstruction
  (#113).
- **build/ci** **Fuzz harness batch 2 (#117)** — `pic`, `cb8`, `raw`,
  `fnt`, `seq`, `audio`, with synthetic seed corpora and format
  dictionaries, wired into the per-PR smoke and weekly deep runs.
- **docs-site** A rebuilt documentation experience: modernized theme with
  print/PDF export (#343) and a "Start Here" learning path with
  concept-first navigation (#344).

### Changed
- **docs** Explanatory byte-layout diagrams for the format specs (#345),
  and shell-neutral modding recipes (#346).
- **ci** `cpp/path-injection` is rule-scoped out of `cli/` in the CodeQL
  filter (#382): an argv path reaching a file open is a command-line
  tool's contract, not injection. The rule stays live for `lib/`, where
  archive-entry-name paths are a real extraction surface.

### Fixed
- **fx-lib** **Five memory-safety bugs in the image/A-V decoders**, found
  by the new fuzz harnesses and fixed with regression tests (#117): two
  32-bit-overflow bounds checks in `pic_decode` (the pixel and palette
  offsets) that let an out-of-range offset read outside the buffer; a
  `cb8_decode_frame` mode-bitmap guard that checked bits but not whole
  words; an unbounded `fnt_repack` glyph-body walk past a truncated code
  section; and a `wav_to_pcm` `fmt ` chunk that read 16 bytes past a
  truncated file.

## [0.5.9] - 2026-07-05

Checkpoint release for Phase 4 (Codec & Test Completeness): **every documented
FA format now has an `fx_lib` codec and `fx` CLI surface** (epic #49), and the
fuzzing rollout is live in CI. Round-trip where the format warrants it,
validated against a real install; read + rationale for the engine-code
overlays and one-way translations.

### Added
- **fx-lib/fx-cli** **Codecs for the last sixteen formats (epic #49).** Every
  documented format `fx` could not previously inspect now has a library codec
  and a CLI command:
  - **Text / config family** (#104) — `TXT`, `CFG`, `DAT`, `MNU`. The `.TXT`
    directive engine is a line-preserving parser that round-trips any input
    byte-identically; `EA.CFG` (347-byte CONFIG struct) and `NET.DAT`
    (3552-byte CN_INFO) round-trip the install's live files through typed
    structs, with the untraced fields passed through verbatim.
  - **Small-binary A** (#108) — `MT`, `PTS`, `RGN`. All 363 `.MT` briefings
    round-trip byte-identically on the shared directive engine; `RGN`
    installer region maps get a full two-way codec.
  - **Small-binary B** (#109) — `SSF` (installer script, byte-identical
    round-trip), `MC`, `HGR`.
  - **XMI → MID** (#106) — a clean-room exporter: the AIL `EVNT` stream (sum
    -of-bytes delay encoding, note-on-with-duration) is decoded and written as
    a Standard MIDI File (format 0). All 78 stock `.XMI` export to valid SMF.
  - **Container inspectors** for the Phar Lap `PL` overlay DLLs — `CAM`/`MNU`
    (#104), `PTS` (#108), `MC`/`HGR` (#109), `DLG` (#105): validated
    container + embedded-string extraction, with the structural record
    decode tracked under #54.
  - **FBC/BIN/CAM** parsers lifted from the GUI into the library (#107).
- **fx-lib** **LIB terminator entry + `ealib_repack` (#115).** Recovered the
  container's directory terminator (the `(N+1)`th all-zero entry whose offset
  is the file size); `fx lib repack` rebuilds any archive from its own
  directory. The full-install round-trip test repacks every `.LIB` in a real
  install byte-identically.
- **build/ci** **Fuzzing rollout (epic #51 in progress).** Container fuzz
  harnesses `fuzz_blast` and `fuzz_pe` (#116), plus a weekly deep fuzz CI job
  (30 min/harness) with an auto-filing finding policy (#119).
- **test** Synthetic-first fixture policy and a dedicated blast-decompressor
  suite (#110, #111).

### Changed
- **repo** Extended the asset gitignore to every documented FA extension so
  game content cannot be committed (#319).

### Fixed
- **fx-lib** `pe_code_section` computed section offsets in 32-bit arithmetic
  that could wrap past the bounds checks into an out-of-bounds read on a
  crafted overlay; found by `fuzz_pe` and now computed in 64-bit (#116).
- **fx-cli** Embed the UTF-8 code-page manifest so non-ASCII file paths work
  on Windows 10 1903+ (#165).

## [0.5.8] - 2026-07-05

### Added
- **fx-lib/fxs** **The SH shape interpreter — complete state-selected rendering (epics #279,
  #295).** `sh_parse_mesh` + `ShState` now select every state dimension a shape carries, and the
  `fxs` orbit view exposes each as a control:
  - **Animation frames** (#302, #304) — `0x40` JumpToFrame interpreted in the base stream, called
    fragments, and x86-gated sub-streams; **Frame** slider.
  - **Damage** — the inline `0xAC` sub-model (#300) *and* the whole-model wreck swap (#314):
    `sh_variant_name` derives the engine-generated `_A`…`_D`/`_S` sibling names, `has_damage`
    reports inline branches, and the **Destroyed** toggle falls back to the `_A.SH` wreck from the
    same LIB — the render-time swap the engine performs for destroyed aircraft.
  - **LOD and detail** (#312) — `0xC8` JumpToLOD levels (synthetic projected-size scalar against
    each site's pixel threshold) and the `0xA6` JumpToDetail preference; **LOD** slider +
    **Low detail** checkbox.
  - **The structural stream walk** (#312), from new engine tracing: `0x1E` is ShortEOF — a
    fragment *return* (`do_short_eof` = `ret`), not a pad; `0x12` Unmask *calls* its sub-stream;
    `0x6C` and the `0x06`/`0x0C`/`0x0E`/`0x10` family are **draw-order selectors** whose both
    sub-chains always render; `0x48`/`0x38` jumps are followed. One coherent state renders instead
    of every frame/LOD/damage block merged — the A-10 yields three clean LODs (377/63/10 faces),
    FA_2.LIB coverage 1257/1275 shapes (98.6%), zero crashes.
  - **Texturing** (#305–#307, #311) — per-face texel coords extracted (+ OBJ `vt`), sampled in
    both render backends, **Texture** toggle resolving the skin PIC from the same LIB, and
    untextured faces shaded by their palette colour instead of flat grey.
  - **Full-model recovery** (#298, #299, #309) — x86-gated articulation geometry recovered via PE
    base-relocation targets, Unmask calls followed, and walk-through harvesting; complete
    airframes (A-10 both halves, AC130 from zero).
- **fx-render** — the shared renderer module chartered in #281: software rasterizer backend
  (#293), OpenGL backend + `fxs` SH preview refactored onto `fx::render` (#294), and texture
  sampling in both backends (#306).
- **fx-lib/re** **`.T2` terrain decoded to the engine's model (#313, closes #262).** The
  "sub-header class constants" are the loader's field map (`T_Load`/`T_GetLeaf`): the payload is
  two flat row-major arrays — the leaf grid plus a per-tile far-LOD summary array — not 195-byte
  tiles; the codec is rewritten to validate both array extents and T2.md flips to **complete**.
  Same PR resolves `T_HANDLE` flag `0x1000`: the vestigial Mac-heritage purged-handle mark (its
  readers survive; the purger `MMCompactRAM` is stubbed on Win32).
- **re** `sh_op_78` characterized as an oriented bounding-box visibility cull (8-corner
  Cohen–Sutherland trivial-reject; emits no geometry) (#310).

### Changed
- **roadmap** mid-2026 realignment: reconstruction folded into Phase 5, `fxe`/`fx_render`
  chartered, interleaved release train (#286); "FA.EXE" genericized to "the game executable" and
  architecture.md reframed as the reconstruction hub (#287); fxs Studio direction framed as
  entity-based editing (#303); epic #279 marked complete in the epic index (effect-data → #315)
  (#316).

### Fixed
- **fxs** SH textures mapped upside-down: SH texel `t` is bottom-left origin — flip V against the
  top-left decoded PIC (#308).

## [0.5.7] - 2026-07-04

### Added
- **re** **The overlay-binary reconstruction program (epic #247) — per-binary tooling and all
  six companion binaries.** The `db/` machinery is now **per-binary** (#252): VA-uniqueness,
  claims, coverage, and the reconstruction matrix are scoped by the `subsystems.csv` `binary`
  column, inventory lives under `db/inventory/<binary>/`, `ExportInventory.java` derives image
  bounds from the program (not a hardcoded window), the launchers take a `[BINARY]` arg, and a
  multi-binary `check_status` self-test guards it — VAs are unique only *within* a binary
  (IP.EXE bases at the same `0x00400000` as FA.EXE; the comms DLLs all at `0x10000000`). On top
  of it, every companion binary FA ships is now documented:
  - **WAIL32.DLL (#253)** — the Miles Sound System (AIL) audio library: 130 public `AIL_*`
    exports named, internals waived (third-party boundary).
  - **IP.EXE (#254)** — re-characterized as an MFC-based **EA system-info / tech-support tool**
    (CD-ROM benchmark, hardware/OS/network profiling, faxes/e-mails a config report to EA) — *not*
    a TCP/IP transport; app logic named, MFC framework waived.
  - **Comms suite CDRV\*32 / COMMSC32 (#255)** — a third-party **Cdrv** serial / modem /
    file-transfer / terminal middleware library: 142 exported ABI functions named, internals waived.
  - **external-imports.md (#260)** — the FA-side boundary to the MS / third-party DLLs
    (DDRAW, WINMM, MSAPI, DSOUND, …), built from the PE import tables; surfaced **`MSAPI.dll` as
    the real matchmaking / internet-play client** (now tracked as epic #272).
- **re** **VIEW subsystem (#257)** — named the in-flight camera & replay cluster
  (`0x40D7A0–0x40F6B0`, 19 functions) the `game-loop.md` refresh surfaced: the external/spot view
  builder, camera-from-object, slew, and the flight replay recorder. FA.EXE is now **20/20
  subsystems, 1728/1728 in-scope functions named**.

### Changed
- **re** **docs/fa open-items closure completed (#247).** Pulled the DLG record-type layouts
  (#258, advancing #54) and the VDO/Cobra corrections (#259, advancing #55) into the sweep, closed
  the remaining #250 subsystem questions, and resolved the GG_Flush DB-split (#262). Every
  `docs/fa` open item is now homed.
- **re** The **reconstruction matrix is multi-binary** — one section per binary, program totals
  across all seven (FA.EXE + the six companions), 26/26 subsystems complete.

### Notes
- **Documentation + tooling release.** `fx_lib` and the `fx` CLI are byte-identical to v0.5.6 —
  no fa-bridge submodule bump. This closes out epic #247's overlay stream: the key finding is that
  all three companion-binary categories are **third-party middleware** (Miles, MFC/EA-tool, Cdrv),
  so the treatment is boundary documentation — the export ABI named, internals waived — not deep
  reverse-engineering. Remaining under #247: the `#262` deep tail (`sh_op_78` geometry, `T_HANDLE`
  flag `0x1000`, `.T2` sub-header) and the **MSAPI.dll matchmaking-client reconstruction** (epic
  #272, the one genuinely game-relevant external found via #260).

## [0.5.6] - 2026-07-04

### Changed
- **re** **docs/fa open-items closure — Phase 1 (#247).** Closed open questions across the
  subsystem docs against the now-complete FA.EXE reconstruction:
  - **game-loop.md refresh (#249):** cleared the stale `Unresolved`/`FUN_` markers — the frame
    timer globals now carry real VAs (`_timerTicks` `0x5528EC`, …), three dispatch calls
    resolve to `MISSIONLoadOrdIcons` / `MAPClearHover` / `MPEnqueue`, and the `_GVProc` /
    `_PROJProc` "callers unresolved" notes are explained as indirect proc-table dispatch.
  - **subsystem sweep (#250), 10 evidence-based resolutions:** fuel flow reads the `_cgt`
    engine-type record (not a global); the object `+0x68` service key is an absolute
    `_currentT` tick saturated by `TimeAddSat`; `HUDDrawTargetView` is a target-slaved 3D
    render and `_hudMasterMode` a 0–6 store-flag enum; `0x4869A0–0x486E60` is the TIME/FPS
    timing cluster; the weapons `0x58F1xx` globals are lock-timing slots; `_ctCheckPass` has no
    writer in FA.EXE (dormant validator) and `_ctState+0x7C` is save/restore-only; `PollMod`
    is the pause/resume mixer service; the joystick calibration record is `0x34` bytes/device.
  - **status cleanups (#251):** seq `SEQUENCE`/`SEQGR` maps homed to the #230 struct-typing
    pass; wingman `_wmFormControl` routed to the re-gameplay epic #56.

### Notes
- **Documentation-only release.** `fx_lib` and the `fx` CLI are byte-identical to v0.5.5 — no
  fa-bridge submodule bump. First cut of the epic #247 docs-closure stream. The game-loop
  refresh surfaced a previously unmapped in-flight **view/replay cluster** (`0x40D7F0–0x40F5D0`),
  filed as a new discovery (#257); a Ghidra xref / raw-byte pass on the remaining subsystem
  questions and the DLG (#258) / VDO (#259) format items continue under #247.

## [0.5.5] - 2026-07-04

### Added
- **re** **Datatype layer for the symbol database (#230).** `db/symbols` gains a `type`
  column, `db/types/fa_types.h` holds the recovered struct layouts (scalar aliases, the
  struct type-vocabulary, and a gcc-verified `CN_INFO`), and `scripts/ghidra/ApplyTypes.java`
  applies both to the Ghidra project. 32 named globals typed by demangling their FA.SMS
  names (`?curSeq@@3FA` → `s16`, `?seqGrList@@3PAUSEQGR@@A` → `SEQGR *`) — the enabling step
  for generating C++ declarations for a clean-room reconstruction (`fc`)

### Changed
- **re** **symbols.md and globals.md are now generated from the symbol database (#229).**
  Their per-subsystem sections come from `db/symbols/` between `<!-- BEGIN/END GENERATED -->`
  markers, written by `--write-matrix` and currency-enforced by `--check` — the same
  mechanism as the reconstruction matrix, so the human-readable registries can no longer
  drift from the database

### Notes
- **Documentation + tooling release.** `fx_lib` and the `fx` CLI are byte-identical to
  v0.5.4 — no fa-bridge submodule bump. Completes the two enhancement follow-ons under the
  now-complete FA.EXE reconstruction (#209); the overlay-binary program and a `docs/fa`
  open-items closure sweep continue under epic #247

## [0.5.4] - 2026-07-04

### Added
- **re** **The `.SEQ` cutscene player — the 19th engine subsystem (#240).** The
  reproducibility audit surfaced a real subsystem the original 18-subsystem map missed: the
  scripted intro/outro sequence player. Its defining trait is that script commands are
  dispatched **by name** (`_SeqContinue` builds `"SEQ"+cmd` and resolves it through
  `_SMAddress`, the FA.SMS symbol map), so the twelve `_SEQ<verb>` handlers are reached only
  by name and never by a direct call — invisible to a purely xref-driven inventory until
  their `source=sms` rows materialise them. Named + documented ([seq.md](https://github.com/jomkz/fighters-codex/blob/main/docs/fa/seq.md) + diagram)
- **re** **SPX/IPX network transport (#241).** The same name-dispatch pattern hid a cluster
  of SPX/IPX transport functions from the network subsystem; the 9 FA.SMS-named leaves plus
  the recovered `spxopensocket` are now claimed and documented

### Changed
- **re** **The reproducibility residue is characterized down to 26 (from 244), with zero
  hidden state** — a from-scratch rebuild from `db/` alone discovers nothing the committed
  inventory lacks. The remaining 26 are benign Ghidra auto-analysis variance (3 tiny stubs,
  1 non-deterministic thunk, 22 data-markup globals), not name drift or `db/` gaps; the
  rationale is recorded in [db/reproducibility-audit.md](https://github.com/jomkz/fighters-codex/blob/main/db/reproducibility-audit.md)

### Notes
- **Documentation + tooling release.** `fx_lib` and the `fx` CLI are byte-identical to
  v0.5.3 — no fa-bridge submodule bump. With the 19th subsystem and the SPX gap closed, the
  FA.EXE reconstruction is complete: **19/19 subsystems, 1709/1709 in-scope functions named**
  (see the [reconstruction matrix](https://github.com/jomkz/fighters-codex/blob/main/docs/fa/reconstruction.md))

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

[Unreleased]: https://github.com/jomkz/fighters-codex/compare/v0.5.9...HEAD
[0.5.9]: https://github.com/jomkz/fighters-codex/releases/tag/v0.5.9
[0.5.8]: https://github.com/jomkz/fighters-codex/releases/tag/v0.5.8
[0.5.7]: https://github.com/jomkz/fighters-codex/releases/tag/v0.5.7
[0.5.6]: https://github.com/jomkz/fighters-codex/releases/tag/v0.5.6
[0.5.5]: https://github.com/jomkz/fighters-codex/releases/tag/v0.5.5
[0.5.4]: https://github.com/jomkz/fighters-codex/releases/tag/v0.5.4
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
