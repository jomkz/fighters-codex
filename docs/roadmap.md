# Roadmap

Development is tracked through [GitHub milestones](https://github.com/jomkz/fighters-codex/milestones),
one per phase. Each phase is a set of `epic`-labeled issues whose sub-issues carry the work.
This document is the map; the tracker is the source of truth for status.

## Charter

The RE documentation is the primary output — format specs, architecture notes, recovered
symbols. The `fx_lib` library, `fx` CLI, and **`fxs`** (**Fighters Studio**, the OpenGL GUI) are
the **format** validation layer: a working, byte-identical codec is the proof that a format is
truly understood. Its **executable** counterpart is **`fxe`** (the **fx-engine**) — a clean-room
modern C++ source port generated from the reconstruction: a port that runs the original content is
the proof that the game executable is understood.
(Restated in [#33](https://github.com/jomkz/fighters-codex/pull/33); see
[README](https://github.com/jomkz/fighters-codex/blob/main/README.md).)

> **`fxs` direction (Fighters Studio):** beyond today's filesystem-style 3-panel LIB/entry chooser,
> `fxs` grows an **entity-based editing** surface. A game *entity* — an aircraft, a campaign, a
> mission, a loadout — is not a single file but an **aggregate of many LIB records**: e.g. an aircraft
> ties together its `.SH` shape(s), `.PT` flight model, `.PIC` textures, `.JT`/`.SEE`/`.ECM` weapon and
> countermeasure records, cockpit art, and audio. The Studio surface presents and edits that whole
> entity as one first-class object — resolving and cross-linking its constituent files — instead of
> requiring the user to edit each raw record in isolation. The existing filesystem chooser stays as the
> low-level view. Tracked as its own scope under the fxs/GUI epics.

## The 1.0 definition

v1.0.0 ships when, and only when:

1. **Every format and subsystem spec is complete**, or its remaining gaps are explicitly
   documented as *requires-gameplay-evidence* or *unrecoverable*.
2. **Every codec is round-trip-proven** (byte-identical repack) **or carries a written
   rationale** for being one-way (e.g. OBJ→SH is intentionally out of scope).
3. **Every codec has tests, fixtures, and a fuzz target**, enforced by the status matrix.
4. **The documentation is restructured (#38) and published** as a docs site.
5. Releases are current; the project then enters maintenance mode.

The Phase 2 **status matrix** (#82) is the audit instrument: the 1.0 audit (#148) is a
mechanical check against it.

## Phases

Phases are **gated by ordering constraints, not dates** — week numbers drift from reality;
gates don't.

```
P0 ─→ P1 ─→ P3 (gui port)
 │     ├──→ P4 (codecs/tests/fuzz) ──┐
 └──→ P2 (docs system) ─────────────┼──→ P5 (deep static RE + reconstruction) ──→ P6 (gameplay + multi-game + 1.0)
                                    ┘
Reality check (mid-2026): the P5 reconstruction ran *ahead* of the P4 validation layer — the game
executable and its overlay binaries are fully named and documented while the codecs/tests/fuzz work
is barely started. P4 and the P5 forward programs (fxe, fx_render) now **interleave**
release-to-release, so neither the docs nor the validation layer goes stale.
P5d (VDO, #55) may start any time after P1 — it is the schedule's long pole.
The RE workbench migration (#120) may start any time after P0.
ATF/USNF acquisition (#144) is external — start immediately.
```

| Phase | Milestone | Goal | Exit gate |
|---|---|---|---|
| **0 — Program Reset** | [Phase 0](https://github.com/jomkz/fighters-codex/milestone/1) | Roadmap live, claims true, release tooling portable, v0.3.0 shipped | Structure instantiated; README contradicts nothing |
| **1 — Cross-Platform Core & CI** | [Phase 1](https://github.com/jomkz/fighters-codex/milestone/2) | fx_lib + fx + tests green on Linux (GCC/Clang) and Windows (MSVC); ctest, sanitizers, CodeQL, fuzz scaffold in CI | Green matrix incl. tests; `fx` output on Fedora byte-identical to Windows |
| **2 — Documentation System** | [Phase 2](https://github.com/jomkz/fighters-codex/milestone/3) | #38 executed: one spec template, all 44 specs restructured, status matrix CI-enforced, docs site published | Site live; matrix drift fails CI |
| **3 — fxs Port** | [Phase 3](https://github.com/jomkz/fighters-codex/milestone/4) | SDL3 + OpenGL3 + miniaudio backends; ready-now validation features | Parity on Fedora + Windows; in CI + releases |
| **4 — Codec & Test Completeness** | [Phase 4](https://github.com/jomkz/fighters-codex/milestone/5) | Round-trip upgrades, codecs for the 16 uncovered formats, full test/fixture coverage, fuzzing rollout | Matrix: every format round-trips or carries a rationale, with tests + fixtures + fuzz target |
| **5 — Deep Static RE + Reconstruction** | [Phase 5](https://github.com/jomkz/fighters-codex/milestone/6) | SH/renderer semantics, format-unknown closure, VDO/Cobra decoded, **and the full reconstruction programs** (game executable + overlay binaries named & documented); forward programs `fxe` and `fx_render` begin here | Static analysis exhausted; the docs alone implement a bridge or a source port |
| **6 — Gameplay, Multi-Game & 1.0** | [Phase 6](https://github.com/jomkz/fighters-codex/milestone/7) | PLT gap campaign on the Windows bench, ATF/USNF verification, 1.0 audit | The 1.0 definition above; v1.0.0 tagged |

## Epic index

| Phase | Epic | Theme |
|---|---|---|
| 0 | [#41](https://github.com/jomkz/fighters-codex/issues/41) | Program reset — roadmap, truth pass, portable tooling, v0.3.0 |
| 1 | [#42](https://github.com/jomkz/fighters-codex/issues/42) | fx_lib + fx CLI + tests Linux port |
| 1 | [#43](https://github.com/jomkz/fighters-codex/issues/43) | CI matrix, sanitizers, CodeQL, fuzz scaffold |
| 2 | [#44](https://github.com/jomkz/fighters-codex/issues/44) | Format spec restructure (#38) |
| 2 | [#45](https://github.com/jomkz/fighters-codex/issues/45) | Status matrix + published docs site |
| 3 | [#46](https://github.com/jomkz/fighters-codex/issues/46) | fxs cross-platform port (supersedes #39) |
| 3 | [#47](https://github.com/jomkz/fighters-codex/issues/47) | GUI validation features — ready-now set |
| 4 | [#48](https://github.com/jomkz/fighters-codex/issues/48) | Round-trip upgrades for one-way codecs |
| 4 | [#49](https://github.com/jomkz/fighters-codex/issues/49) | Codecs for the 16 uncovered formats |
| 4 | [#50](https://github.com/jomkz/fighters-codex/issues/50) | Test & fixture completeness |
| 4 | [#51](https://github.com/jomkz/fighters-codex/issues/51) | Fuzzing rollout |
| 4 | [#279](https://github.com/jomkz/fighters-codex/issues/279) | fx_lib asset interpreters — SH geometry + effect data |
| 4 | [#281](https://github.com/jomkz/fighters-codex/issues/281) | fx_render — shared render abstraction (OpenGL + software backends) |
| 5 | [#53](https://github.com/jomkz/fighters-codex/issues/53) | Renderer & effects internals — residual static closure |
| 5 | [#54](https://github.com/jomkz/fighters-codex/issues/54) | Format-unknown closure (static) — residual tail |
| 5 | [#55](https://github.com/jomkz/fighters-codex/issues/55) | VDO/Cobra video — the long pole |
| 5 | [#209](https://github.com/jomkz/fighters-codex/issues/209) | Game-executable reconstruction — every function/variable named & documented (complete) |
| 5 | [#247](https://github.com/jomkz/fighters-codex/issues/247) | Overlay-binary reconstruction — WAIL32 / IP.EXE / comms DLLs (complete) |
| 5 | [#272](https://github.com/jomkz/fighters-codex/issues/272) | MSAPI.dll — matchmaking / internet-play client reconstruction |
| 5† | [#280](https://github.com/jomkz/fighters-codex/issues/280) | fxe — clean-room modern C++ source port of the game executable |
| 6 | [#56](https://github.com/jomkz/fighters-codex/issues/56) | Gameplay-gated RE (Windows bench) |
| 6 | [#57](https://github.com/jomkz/fighters-codex/issues/57) | ATF/USNF verification pass |
| 6 | [#58](https://github.com/jomkz/fighters-codex/issues/58) | v1.0 audit, release, maintenance mode |

† `fxe` (#280) has its own [milestone](https://github.com/jomkz/fighters-codex/milestones), sequenced
out of the Phase 5 reconstruction and interleaved with Phase 4; it is a stretch program, not a 1.0 gate.
The SH-semantics epic (#52) and the two reconstruction milestones are complete and folded in here.

Standalone Phase 5 prerequisite: [#120](https://github.com/jomkz/fighters-codex/issues/120)
— migrate the RE workbench (Ghidra project + FA corpus) to Fedora.

Phase 5 folds in the two **reconstruction programs** — a per-subsystem lens (naming + documentation
+ diagrams) over the same code the static-RE epics decode — plus the forward programs (`fxe`,
`fx_render`) they enable. Both reconstruction programs are **complete**.

### Program: Game-executable reconstruction

Epic [#209](https://github.com/jomkz/fighters-codex/issues/209) is the long-horizon goal of a
*complete understanding* of the game executable: every function and variable named in the Ghidra
project, and every subsystem documented in `docs/fa/` with recovered symbols, struct maps, and an
SVG flow diagram.

**Complete (as of v0.5.7): all 20 mapped subsystems are `complete`** — 1,728 in-scope functions
named and every referenced global named or waived, each with a `docs/fa/` page and a theme-aware
SVG. See the [reconstruction matrix](fa/reconstruction.md). A from-scratch
[reproducibility audit](https://github.com/jomkz/fighters-codex/blob/main/db/reproducibility-audit.md)
(rebuild the project from `db/`, diff against the committed inventory) confirms **zero name drift**
and that `db/` fully drives the named project — that audit surfaced the final subsystems the original
map missed (the `.SEQ` cutscene player [#240](https://github.com/jomkz/fighters-codex/issues/240) and
the in-flight VIEW camera/replay cluster [#257](https://github.com/jomkz/fighters-codex/issues/257)).

The program is driven by a machine-readable **symbol database** under
[`db/`](https://github.com/jomkz/fighters-codex/blob/main/db/README.md): a manifest of
the subsystems, per-subsystem `symbols/*.csv` files (the canonical VA→name record,
applied to the Ghidra project by `scripts/ghidra/ApplySymbols.java` and checked against
the committed inventory export), and the generated
[reconstruction matrix](fa/reconstruction.md). `check_status.py` enforces, per completed
subsystem, that **every code-referenced function is named and every referenced global is
named or waived** (the referenced-globals rule), plus the subsystem doc's structure and
theme-aware diagram. The definition of done is exemplified by
[objects.md](fa/objects.md) + [shape-selection.md](fa/shape-selection.md). The same database is the
substrate for the **fxe** source port (below).

### Program: Overlay Reconstruction

Epic [#247](https://github.com/jomkz/fighters-codex/issues/247) is the sibling program: the same
treatment — every function/variable named, every subsystem documented — applied to the binaries the
game ships alongside the executable. Unlike the main executable these ship no `.SMS` symbol map, so
naming seeds from the DLL export/import tables, strings, and RTTI. The same
[`db/`](https://github.com/jomkz/fighters-codex/blob/main/db/README.md) machinery drives it — the
`subsystems.csv` `binary` column and a per-binary `db/inventory/<binary>/` export scope every check
to one image (VAs collide across binaries — `IP.EXE` bases at the same `0x00400000` as the main
executable; the comms DLLs all at `0x10000000`), and each binary gets its own section in the
[reconstruction matrix](fa/reconstruction.md). **Complete** across 7 binaries. Key findings from the
RE (correcting earlier assumptions):

- **WAIL32.DLL** = the Miles Sound System (AIL) audio middleware — boundary-documented (exported API
  named, third-party internals waived).
- **IP.EXE** = an EA system-info / tech-support tool (MFC), **not** a TCP/IP transport as first
  assumed — no Winsock.
- The comms/modem drivers (`CDRV*32.DLL`, `COMMSC32.DLL`) = third-party `Cdrv` serial/modem
  middleware — boundary-documented.
- The real internet-play / matchmaking client is **MSAPI.dll** (the genuinely game-relevant network
  binary the epic first mistook `IP.EXE` for) — reconstruction tracked as epic
  [#272](https://github.com/jomkz/fighters-codex/issues/272).

The interface to the Microsoft / third-party redistributables is documented at the boundary
([external-imports.md](fa/external-imports.md), [#260](https://github.com/jomkz/fighters-codex/issues/260))
without reversing their code.

### Program: `fxe` — the fx-engine (clean-room source port of the game executable)

Where fx_lib / fx / fxs prove the **format** documentation by processing assets, **fxe** (the
**fx-engine**) proves the **reconstruction** documentation of the game executable by being a runnable,
clean-room, modern C++ **source port** (epic [#280](https://github.com/jomkz/fighters-codex/issues/280),
its own [milestone](https://github.com/jomkz/fighters-codex/milestones)). Give it the content from the
user's original disks and it plays the game the way `FA.EXE` did — rendering through the shared
`fx_render` module with **software and OpenGL** backends (**Vulkan** later) and modern audio. It is **generated** from `db/` + the subsystem docs by an
in-repo generator that is the source of truth; the emitted C++ is committed and kept in sync by CI
(the same pattern as the generated matrices), clean-room from *our own* facts and prose and never
transcribed from decompiler output. Legal model: a **source port** — ship no assets, require the
original disks (documented in an ADR + NOTICE). fxe is a **validation lens**, interleaved with the
Phase 4 train, **not a 1.0 gate**, and independent of fa-bridge.

### Program: `fx_render` — one renderer for three consumers

`fx_render` (epic [#281](https://github.com/jomkz/fighters-codex/issues/281)) is a committed MIT
render-abstraction module — a backend-agnostic geometry→pixels API with **OpenGL** and
**FA-faithful software** backends — extracted from fxs's renderer so the OpenGL/software path is
built once and shared by **fxs** and **fxe** (and available for the fighters-legacy engine to
adopt, rather than a third implementation). The SH/effect *asset* interpretation it renders comes
from fx_lib (epic [#279](https://github.com/jomkz/fighters-codex/issues/279)); the full runtime lives
in fxe.

## Relationship to fighters-legacy

The RE understanding produced here has **two independent consumers**, and this repo produces the
shared pieces both build on:

- **codex** (MIT) produces the RE docs (primary), the **format** validation layer (`fx_lib` / `fx` /
  `fxs`), the shared **`fx_render`** core (OpenGL + software backends), and **`fxe`** — a committed
  clean-room source port of the game executable (generated from `db/` + docs, requires the user's
  original content).
- **[fa-bridge](https://github.com/fighters-legacy/fa-bridge)** (GPL) implements the engine's
  `IContentPack` using `fx_lib` as a submodule and the RE docs; it runs FA content on the
  **fighters-legacy** (GPL) engine, which may itself adopt `fx_render` rather than write a third
  OpenGL/software path.

`fxe` and fa-bridge are **independent** consumers of the same reconstruction — one a standalone MIT
source port, one a GPL bridge — and share no code. The reconstruction being complete unblocks
fa-bridge's former interpreter/rasterizer work (the docs it was waiting on now exist). After each
release, fa-bridge's `extern/fx_lib` submodule is bumped to the tag (the release script prints the
reminder).

**License boundary:** fighters-codex is MIT; OpenFA and fa-bridge are GPL. RE facts are documented
here with attribution where verified against other projects' findings; code is never transcribed
across the boundary. MIT→GPL reuse (fa-bridge/fighters-legacy consuming `fx_lib`/`fx_render`) is
one-way and clean.

## Releases

Minimum one release per phase gate: v0.3.0 (P0) · v0.4.0 (P1) · v0.5.0 (P2+P3) ·
v0.6.0 (P4) · v0.7–v0.9 (P5, as RE lands) · **v1.0.0** (P6).

The v0.5.x train (P5 reconstruction) shipped ahead of P4; from here the P4 validation work and the
P5 forward programs (`fxe`, `fx_render`, residual RE) **interleave** — alternating release-to-release
so neither the docs nor the codec/test layer goes stale. `fxe` is a stretch program on its own
milestone and is **not** a 1.0 gate.

## How this roadmap is maintained

- Phases are milestones; epics are `epic`-labeled issues; work items are native sub-issues.
- New work lands as a sub-issue of the epic it serves, in that epic's milestone.
- Gaps discovered during RE are tagged `re-static` (Ghidra can answer it) or `re-gameplay`
  (needs the Windows bench + running game); Phase 6's bench campaign batches the latter.
- The status matrix (#82) is updated in the same PR as the change it reflects
  (see the docs-currency rule in
  [CLAUDE.md](https://github.com/jomkz/fighters-codex/blob/main/CLAUDE.md)).
- No standalone TODO files — if it's worth doing, it's an issue.
