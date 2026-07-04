# Symbol database

Machine-readable record of the reconstruction programs — FA.EXE
([epic #209](https://github.com/jomkz/fighters-codex/issues/209)) and the overlay/companion
binaries ([epic #247](https://github.com/jomkz/fighters-codex/issues/247)): every recovered
symbol name, the subsystem and **binary** it belongs to, and the Ghidra-project ground truth it
is checked against. The human-readable story lives in [docs/fa/](../docs/fa/README.md); this tree
is what keeps those docs and the Ghidra project mechanically in sync. Everything is scoped by the
`subsystems.csv` `binary` column — VAs are unique only within a binary. Program progress is
tracked in the generated [reconstruction matrix](../docs/fa/reconstruction.md).

The schema is deliberately machine-first: downstream tooling — up to and including
generation of C++ declarations (structs with asserted offsets, per-subsystem
prototypes, dispatch tables) for a clean-room reimplementation — should be able to
consume these files without parsing prose. To that end each symbol row carries a
`type` column, and recovered struct layouts live under [`db/types/`](types/README.md)
(applied by `scripts/ghidra/apply_types.sh`).

`tools/check_status.py --check` validates everything here (schemas, coverage, doc
consistency) in CI and under `ctest -L docs`.

## Workflow

```
edit db/symbols/<slug>.csv            # record/propose names + types (the DB is canonical)
edit db/types/<binary>/*.h            # recovered struct layouts (optional; FA.EXE at db/types/*.h)
scripts/ghidra/apply_symbols.sh  [BINARY]   # apply names to the Ghidra project (default FA.EXE)
scripts/ghidra/apply_types.sh    [BINARY]   # apply db/types/ + the type column
scripts/ghidra/export_inventory.sh [BINARY] # re-export db/inventory/<binary>/ ground truth
python3 tools/check_status.py --write-matrix   # refresh reconstruction.md + registries
python3 tools/check_status.py --check
```

Each launcher takes an optional `BINARY` (the Ghidra program name = imported filename,
e.g. `FA.EXE`, `WAIL32.DLL`; `ALL` loops every binary in `subsystems.csv`), and applies /
exports only that binary — **VAs are unique only within a binary** (IP.EXE bases at the same
`0x00400000` as FA.EXE). Import the overlay/companion binaries with
`scripts/ghidra/import_targets.sh` first. Image bounds come from the program, not a hardcoded
window.

**Canonical-project rule:** the committed inventory is always exported from the
canonical Fedora `fa-re` project (issue #120). The `.bat` launchers exist for the
Windows bench, but a bench project copy is never the source of truth.

## `subsystems.csv` — program manifest

One row per subsystem of the epic's map. Columns:

| Column | Meaning |
|--------|---------|
| `slug` | kebab-case id; names the `db/symbols/<slug>.csv` file |
| `title` | human name, as used in the matrix |
| `binary` | owning binary (`FA.EXE`; overlay subsystems name their DLL/EXE when they start) |
| `ranges` | semicolon-separated `0xAAAAAA-0xBBBBBB` VA ranges that *seed* membership |
| `status` | `planned` → `active` → `complete` (the coverage ratchet — see below) |
| `doc` | repo-relative path of the subsystem doc (must exist once `status != planned`) |
| `issue` | GitHub issue number for the subsystem's sub-issue |

Ranges may overlap between subsystems (the binary interleaves them); a symbol belongs
to the subsystem whose symbols file lists it, and range-based coverage excludes VAs
claimed by another subsystem's file. A symbols file may also claim VAs *outside* its
ranges (e.g. `objects` claims the type-setup slice inside the terrain range).

## `symbols/<slug>.csv` — recovered names

One row per symbol the subsystem claims. Columns:

| Column | Meaning |
|--------|---------|
| `va` | `0x00XXXXXX`, ascending within the file, globally unique across files |
| `kind` | `func` or `data` |
| `name` | the label exactly as applied in Ghidra (mangled FA.SMS names stay mangled) |
| `display` | optional readable form used by the docs (`SetupOT` for `_SetupOT@4`); empty for `re` rows — docs must use recovered names verbatim |
| `source` | `sms` (from FA.SMS), `re` (recovered by this program), `waiver` (deliberately not named — `notes` must say why) |
| `confidence` | `confirmed` or `inferred` ([spec-authoring.md](../docs/spec-authoring.md) vocabulary) |
| `notes` | short role note; **required** for waivers; carries attribution when a name was corroborated externally |
| `type` | optional C type (data: `u32`, `SEQGR *`, `char[16]`; func: a signature override, usually empty). Must resolve to a builtin or a [`db/types/`](types/README.md) declaration; **must be empty on waivers**. See [db/types/README.md](types/README.md) |

Naming convention for `re` rows: follow the subsystem's FA.SMS prefix and casing
(`OBJ…`, `T_…`, `Setup…`) so decompiled code reads uniformly — provenance lives in
`source`, not in the name. Waivers cover things that are *understood but deliberately
unnamed*: interiors of arrays/structs named at their base, switch jump tables, CRT
helpers and thunks.

**License boundary:** names corroborated against OpenFA (GPL) carry an attribution
note in `notes` — facts with attribution, never transcription
([charter](../docs/roadmap.md#relationship-to-fighters-legacy)).

## `inventory/<binary>/` — Ghidra ground truth (exported, committed)

Fact snapshots regenerated by `scripts/ghidra/export_inventory.sh <binary>`; never edited by
hand. Committing them is what lets CI check coverage without a Ghidra install. Each binary
has its own directory (`db/inventory/FA.EXE/`, `db/inventory/WAIL32.DLL/`, …) — VAs are unique
only within a binary, so coverage is checked per binary.

| File (per `<binary>/`) | Contents |
|------|----------|
| `functions.csv` | `va,name,size` for every function in the program |
| `globals.csv` | `va,name,xref_count,subsystems` for every data symbol with ≥1 code xref; `subsystems` = slugs whose functions reference it |
| `ranges.csv` | `slug,range,bytes,bytes_in_functions,functions` per manifest range — the code-coverage signal that exposes undiscovered code |

## Definition of done (per subsystem, enforced at `status=complete`)

1. **Functions:** every `inventory/functions.csv` entry inside the subsystem's ranges
   (minus VAs claimed by other subsystems, plus VAs this file claims anywhere) appears
   in the symbols file, named or waived; no `FUN_*` names remain.
2. **Data:** every `inventory/globals.csv` row tagged with the subsystem is named or
   waived (the *referenced-globals rule* — zero-xref struct interiors don't count).
3. **Doc:** the manifest `doc` exists, its symbol tables agree with the DB, and it
   embeds a theme-aware SVG flow diagram (structure checked; see
   [spec-authoring.md](../docs/spec-authoring.md)).
