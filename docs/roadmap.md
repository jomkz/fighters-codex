# Roadmap

Development is tracked through [GitHub milestones](https://github.com/jomkz/fighters-codex/milestones),
one per phase. Each phase is a set of `epic`-labeled issues whose sub-issues carry the work.
This document is the map; the tracker is the source of truth for status.

## Charter

The RE documentation is the primary output — format specs, architecture notes, recovered
symbols. The `fx_lib` library, `fx` CLI, and `fx-gui` are the validation layer: a working,
byte-identical codec is the proof that a format is truly understood. (Restated in
[#33](https://github.com/jomkz/fighters-codex/pull/33); see [README](../README.md).)

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
 └──→ P2 (docs system) ─────────────┼──→ P5 (deep static RE) ──→ P6 (gameplay + multi-game + 1.0)
                                    ┘
P5d (VDO, #55) may start any time after P1 — it is the schedule's long pole.
The RE workbench migration (#120) may start any time after P0.
ATF/USNF acquisition (#144) is external — start immediately.
```

| Phase | Milestone | Goal | Exit gate |
|---|---|---|---|
| **0 — Program Reset** | [Phase 0](https://github.com/jomkz/fighters-codex/milestone/1) | Roadmap live, claims true, release tooling portable, v0.3.0 shipped | Structure instantiated; README contradicts nothing |
| **1 — Cross-Platform Core & CI** | [Phase 1](https://github.com/jomkz/fighters-codex/milestone/2) | fx_lib + fx + tests green on Linux (GCC/Clang) and Windows (MSVC); ctest, sanitizers, CodeQL, fuzz scaffold in CI | Green matrix incl. tests; `fx` output on Fedora byte-identical to Windows |
| **2 — Documentation System** | [Phase 2](https://github.com/jomkz/fighters-codex/milestone/3) | #38 executed: one spec template, all 44 specs restructured, status matrix CI-enforced, docs site published | Site live; matrix drift fails CI |
| **3 — fx-gui Port** | [Phase 3](https://github.com/jomkz/fighters-codex/milestone/4) | SDL3 + OpenGL3 + miniaudio backends; ready-now validation features | Parity on Fedora + Windows; in CI + releases |
| **4 — Codec & Test Completeness** | [Phase 4](https://github.com/jomkz/fighters-codex/milestone/5) | Round-trip upgrades, codecs for the 16 uncovered formats, full test/fixture coverage, fuzzing rollout | Matrix: every format round-trips or carries a rationale, with tests + fixtures + fuzz target |
| **5 — Deep Static RE** | [Phase 5](https://github.com/jomkz/fighters-codex/milestone/6) | SH/renderer semantics (pulled in from fa-content), format-unknown closure, VDO/Cobra decoded | Static analysis exhausted; fa-content implementable from the docs alone |
| **6 — Gameplay, Multi-Game & 1.0** | [Phase 6](https://github.com/jomkz/fighters-codex/milestone/7) | PLT gap campaign on the Windows bench, ATF/USNF verification, 1.0 audit | The 1.0 definition above; v1.0.0 tagged |

## Epic index

| Phase | Epic | Theme |
|---|---|---|
| 0 | [#41](https://github.com/jomkz/fighters-codex/issues/41) | Program reset — roadmap, truth pass, portable tooling, v0.3.0 |
| 1 | [#42](https://github.com/jomkz/fighters-codex/issues/42) | fx_lib + fx CLI + tests Linux port |
| 1 | [#43](https://github.com/jomkz/fighters-codex/issues/43) | CI matrix, sanitizers, CodeQL, fuzz scaffold |
| 2 | [#44](https://github.com/jomkz/fighters-codex/issues/44) | Format spec restructure (#38) |
| 2 | [#45](https://github.com/jomkz/fighters-codex/issues/45) | Status matrix + published docs site |
| 3 | [#46](https://github.com/jomkz/fighters-codex/issues/46) | fx-gui cross-platform port (supersedes #39) |
| 3 | [#47](https://github.com/jomkz/fighters-codex/issues/47) | GUI validation features — ready-now set |
| 4 | [#48](https://github.com/jomkz/fighters-codex/issues/48) | Round-trip upgrades for one-way codecs |
| 4 | [#49](https://github.com/jomkz/fighters-codex/issues/49) | Codecs for the 16 uncovered formats |
| 4 | [#50](https://github.com/jomkz/fighters-codex/issues/50) | Test & fixture completeness |
| 4 | [#51](https://github.com/jomkz/fighters-codex/issues/51) | Fuzzing rollout |
| 5 | [#52](https://github.com/jomkz/fighters-codex/issues/52) | SH engine-behavior semantics |
| 5 | [#53](https://github.com/jomkz/fighters-codex/issues/53) | Renderer & effects internals |
| 5 | [#54](https://github.com/jomkz/fighters-codex/issues/54) | Format unknown closure (static) |
| 5 | [#55](https://github.com/jomkz/fighters-codex/issues/55) | VDO/Cobra video — the long pole |
| 6 | [#56](https://github.com/jomkz/fighters-codex/issues/56) | Gameplay-gated RE (Windows bench) |
| 6 | [#57](https://github.com/jomkz/fighters-codex/issues/57) | ATF/USNF verification pass |
| 6 | [#58](https://github.com/jomkz/fighters-codex/issues/58) | v1.0 audit, release, maintenance mode |

Standalone Phase 5 prerequisite: [#120](https://github.com/jomkz/fighters-codex/issues/120)
— migrate the RE workbench (Ghidra project + FA corpus) to Fedora.

## Relationship to fighters-legacy

[fa-content](https://github.com/fighters-legacy/fa-content) implements the engine's
`IContentPack` using `fx_lib` as a submodule. Per the charter, **reverse-engineering
documentation lives here; implementations that consume it live there** — fa-content's former
RE issues (C.1–C.5) moved into Phase 5 epics #52/#53, and its interpreter/rasterizer issues
are blocked on those docs. Phase 1 (fx_lib on Linux) is what unblocks fa-content's bridge
work. After each release, fa-content's `extern/fx_lib` submodule is bumped to the tag
(the release script prints the reminder).

**License boundary:** fighters-codex is MIT; OpenFA and fa-content are GPL. RE facts are
documented here with attribution where verified against other projects' findings; code is
never transcribed across the boundary.

## Releases

Minimum one release per phase gate: v0.3.0 (P0) · v0.4.0 (P1) · v0.5.0 (P2+P3) ·
v0.6.0 (P4) · v0.7–v0.9 (P5, as RE lands) · **v1.0.0** (P6).

## How this roadmap is maintained

- Phases are milestones; epics are `epic`-labeled issues; work items are native sub-issues.
- New work lands as a sub-issue of the epic it serves, in that epic's milestone.
- Gaps discovered during RE are tagged `re-static` (Ghidra can answer it) or `re-gameplay`
  (needs the Windows bench + running game); Phase 6's bench campaign batches the latter.
- The status matrix (#82) is updated in the same PR as the change it reflects
  (see the docs-currency rule in [CLAUDE.md](../CLAUDE.md)).
- No standalone TODO files — if it's worth doing, it's an issue.
