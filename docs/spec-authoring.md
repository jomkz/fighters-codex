# Format Spec Authoring Guide

How to write and restructure the format specs in [docs/fa/formats/](fa/formats/README.md).
Every spec follows one template — general reference for modders at the top, deep
technical spec below — and carries machine-readable YAML front-matter that feeds the
generated [status matrix](fa/formats/STATUS.md). `tools/check_status.py` enforces
everything on this page in CI (`--check`); the canonical vocabularies (section names,
enums) are constants in that script, and this page documents them.

```
python3 tools/check_status.py --self-test      # checker's own test suite
python3 tools/check_status.py --check          # what CI runs
python3 tools/check_status.py --write-matrix   # regenerate STATUS.md after edits
```

## The template

````markdown
---
format: XXX                  # uppercase token == filename stem
name: Human Name             # short; the H1 must match
extensions: [".XXX"]
category: graphics           # see category table below
endianness: little           # little | big | mixed | none (plain-text formats)
spec:
  status: partial            # complete | partial | stub
  gaps:                      # required unless status: complete
    - kind: re-static        # re-static | re-gameplay | unrecoverable
      issue: 54              # tracking issue (omit only for unrecoverable)
      note: "short label shown in the matrix"
codec:
  direction: round-trip      # none | read | write | round-trip
  byte_identical: true       # round-trip only; true requires a proving test
  lib: [lib/src/xxx.cpp]     # every path is existence-checked by CI
  commands: [xxx]            # fx subcommand tokens (dispatch literals)
  tests: [tests/test_xxx.cpp]
  fuzz: []
  fixtures:
    synthetic: true          # tests build their own inputs
    real_manifest: false     # extension appears in fa-extract.sha256
related: [LIB]               # tokens; each must be linked in the body
---

# XXX — Human Name (.XXX)

Intro prose (required, no heading): what the format is, which .LIBs or install
paths contain it, container notes. This is the modder-facing overview.

## Tools

### fx

fx command examples. Required section whenever codec.commands is non-empty.

### Other Tools

External tools, free ones plain, paid ones marked `$`.

## File Layout

All multi-byte integers are little-endian unless noted.   <- first line, binary formats

| Offset | Size | Type | Description |
|--------|------|------|-------------|
| `0x00` | 4    | u32  | ...         |

Free H3 structure below (### Header, ### Records, ### Calibration, ...).
Text/DSL formats describe grammar and record structure here instead.

## File Inventory

Known instances in the game, counts, per-file tables. Optional.

## Engine Notes

Confirmed engine functions (VAs + FA.SMS symbols), runtime behavior. Optional.

## Round-Trip Notes

What re-encoding canonicalises, one-way rationale prose, parity caveats. Optional.

## Open Questions

Required iff spec.gaps is non-empty; forbidden when status: complete.

### 1. Short gap title

Evidence so far, what was tried, what would close it.

*Status: open — re-static (#54)*

## Related

**Formats:** [LIB](LIB.md) — ...
**Engine:** [architecture.md](../architecture.md) — ...
````

Only the intro prose, `## File Layout`, and `## Related` are unconditional.
A trivially small format needs nothing else — no empty boilerplate sections
(see [FBC.md](fa/formats/FBC.md)). H2s must come from the canonical set above,
appear at most once, and keep that relative order. [LIB.md](fa/formats/LIB.md)
is the full-complexity exemplar.

## Vocabularies

**Categories** (front-matter `category` → section of
[formats/README.md](fa/formats/README.md) the spec must be indexed under):

| Token | README section | | Token | README section |
|---|---|---|---|---|
| `archive` | Archive | | `mission` | Mission & Campaign |
| `graphics` | Graphics & Images | | `typedef` | Type Definitions (BRF DSL) |
| `terrain` | Terrain & Maps | | `ui-overlay` | UI & Win32 Overlays |
| `3d` | 3D & Scene | | `system` | System & Config |
| `audio` | Audio | | `installer` | Installer |
| `video` | Video & Cutscenes | | `text` | Text |

**Confidence markers** (three levels, used everywhere in docs/fa/):

- **confirmed** — decompile evidence with the VA cited, or proven by a
  byte-identical round-trip
- **inferred** — consistent with all observed data, but not traced in FA.EXE
- **unknown** — unmapped

Region headings take a suffix (`### Identity block (0x00–0xAF) — confirmed`);
unknown fields get Type `?` and a Description starting `**Unknown**`, optionally
followed by the hypothesis. Do not use the retired spellings (Status: unmapped,
unresolved, TBD, requires trace, High/Low).

**Gap taxonomy** (`spec.gaps[].kind`, from the [roadmap](roadmap.md)):

- `re-static` — Ghidra static analysis can answer it; issue under epic #54
- `re-gameplay` — needs the running game on the Windows bench; issue under #56
- `unrecoverable` — provably lost (document why in the Open Questions entry)

Every `## Open Questions` entry is a numbered H3 ending with a status line that
names the gap's issue: `*Status: open — re-static (#123)*`.

**Codec claims** (`codec.*`): `direction` is the fx_lib state — `none` (no codec;
give `issue` pointing at the Phase-4 coverage work, or a `rationale` if none will
ever exist), `read`/`write` (one-way; `rationale` if by design, else `issue` for
the upgrade), `round-trip` (+ `byte_identical`). Once a codec is byte-identical
round-trip, `rationale`/`issue` are forbidden — there is nothing left to track.
All `lib`/`tests`/`fuzz`/`gui` paths and `commands` tokens are verified against
the repository; once all specs are converted, the reverse also holds (every
codec, test, fuzz harness, CLI command, and GUI editor must be claimed by a spec).

**Offset tables** — one style, markdown tables with backticked hex offsets:
`| Offset | Size | Type | Description |`. Record-relative offsets use `+0x0E`.
Types: `u8 u16 u32 s8 s16 s32 f32 char[N] ?`. State endianness once, in the
File Layout preamble; per-field suffixes (`u16 BE`) only for deviations.
BRF-DSL annotated listings of *file content* stay as fenced code blocks — they
are examples, not layout tables.

## Migration map (restructuring a legacy spec)

| Old section | Goes to |
|---|---|
| Overview / Location / Found in | intro prose |
| Format / Structure / File Layout / Header | `## File Layout` |
| Calibration (BRF family) | `### Calibration` under File Layout |
| Applications / fx commands | `## Tools` |
| Confirmed Engine Functions / Playback Architecture | `## Engine Notes` |
| Pending Trace / gap-status blocks | `## Open Questions` + `spec.gaps` |
| Toolkit Roadmap | **deleted** — file the items as issues under the epic they serve |
| Related / Related Formats | `## Related` |

Restructuring is content-preserving: no sentence is deleted unless it is
duplicated or migrated to a quoted issue. Review batch diffs with
`git diff --color-moved=dimmed-zebra`. After converting a spec, delete its token
from `LEGACY` in `tools/check_status.py` and run `--write-matrix` — CI fails
otherwise, in both directions.

## Worked front-matter for the odd cases

Every irregular shape in the corpus, so no conversion has to invent policy:

- **Multiple extensions, one format** — 11K:
  `extensions: [".11K", ".5K", ".8K"]`, `commands: [audio]`.
- **Multiple variant files, one format** — CFG (`variants: ["EA.CFG", "IP.CFG"]`)
  and DAT (`variants: ["NET.DAT", "MODEM.DAT", "SERIAL.DAT"]`); a variant's
  layout is an H3 under File Layout, never a second H1.
- **Sub-format variants** — PIC: `variants: ["dense", "sparse", "jpeg"]`.
- **Two specs, one codec** — M and MM both claim `lib: [lib/src/mission.cpp]`
  and `commands: [mission]`; MM additionally claims its alias `mm`. Shared
  paths are expected; claims are a many-to-many mapping.
- **Family umbrella** — BRF: `extensions: []` plus `family: BRF`; the seven
  member specs (OT NT PT JT SEE ECM GAS) each set `family: BRF`, claim
  `lib: [lib/src/brf.cpp, lib/src/ot.cpp]`, their own dispatch token
  (`commands: [pt]`), and `tests: [tests/test_brf.cpp]`.
- **CLI-only handler** — MUS: `direction: read`, `lib: []`,
  `commands: [mus]` (the parser lives in `cli/cmd_mus.cpp`; lifting it into
  fx_lib is #101).
- **GUI-only viewer, no codec** — FBC: `direction: none`, `issue: 107`,
  `gui: [gui/src/editors/vdo_editor.cpp]`.
- **One-way by design** — SH: `direction: read`,
  `rationale: "OBJ export is intentionally one-way (#48: no OBJ→SH encoder planned)"`.
- **Compiler pair** — `direction` describes fx_lib's ability on *this* format:
  AI is `read` (`fx ai compile` fully parses `.AI`; nothing writes it until
  the #102 decompiler), while BI is `round-trip` with `byte_identical: false`
  (`fx ai compile` writes it, `fx bi dump` reads it, dump→recompile is not
  byte-exact — #102).
- **Big-endian** — XMI: `endianness: big` (IFF-style chunk sizes); the File
  Layout preamble states it.
- **Project artifact, not game data** — SMS (the recovered symbol map):
  documented like any other format; `category: system`.
- **No codec yet** — the uncovered formats reference their Phase-4 issue:
  TXT/CFG/MNU/DAT → #104, DLG → #105, XMI → #106, FBC/BIN/CAM → #107,
  MT/PTS/RGN → #108, SSF/MC/HGR → #109, VDO → #55.

## Front-matter YAML subset

The checker parses front-matter with a deliberately small stdlib-only parser.
Stay within: 2-space indentation, at most two nesting levels, scalars
(bare token, `"quoted string"`, integer, `true`/`false`), inline lists of
scalars (`[a, b]`), and block lists of flat maps (the `gaps` shape). No
anchors, flow maps, multiline scalars, tabs, or inline comments. Full-line
`#` comments are fine. GitHub renders the block as a table at the top of the
spec; a future docs site consumes it as page metadata.
