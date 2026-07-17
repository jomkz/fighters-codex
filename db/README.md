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
consistency) in CI and under `ctest -L docs`. Coverage and matrix currency are
ground-truthed against the [local inventory export](#inventorybinary--ghidra-ground-truth-exported-local-only)
— on machines without it (CI included) those specific checks are skipped with a
printed note; everything else still runs.

## Workflow

```
edit db/symbols/<slug>.csv            # record/propose names + types (the DB is canonical)
edit db/types/<binary>/*.h            # recovered struct layouts (optional; FA.EXE at db/types/*.h)
python3 tools/gen_signatures.py --write     # materialize the signatures FA.SMS names encode
python3 tools/recover_signatures.py --write # recover cdecl signatures from call sites
python3 tools/recover_globals.py --write    # type the globals from their access width
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

**Canonical-project rule:** the inventory export is always taken from the
canonical Fedora `fa-re` project (issue #120); a bench project copy is never the
source of truth. (The Windows `.bat` launcher mirrors were retired in #374 — the
`.sh` launchers are the one supported path.)

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
| `type` | C type (data: `u32`, `SEQGR *`, `char[16]`; func: the full signature, e.g. `char __fastcall COLFlatGround(long, F24_POINT3 *, F24_POINT3 *, F24_POINT3 *)`). Must resolve to a builtin or a [`db/types/`](types/README.md) declaration — `check_status.py` enforces this; **must be empty on waivers**. Func signatures the FA.SMS name already encodes are materialized by `tools/gen_signatures.py`; those the name does not encode are recovered from call-site evidence by `tools/recover_signatures.py` (see [db/types/README.md](types/README.md)) |

Naming convention for `re` rows: follow the subsystem's FA.SMS prefix and casing
(`OBJ…`, `T_…`, `Setup…`) so decompiled code reads uniformly — provenance lives in
`source`, not in the name. Waivers cover things that are *understood but deliberately
unnamed*: interiors of arrays/structs named at their base, switch jump tables, CRT
helpers and thunks.

**License boundary:** names corroborated against OpenFA (GPL) carry an attribution
note in `notes` — facts with attribution, never transcription
([charter](../docs/roadmap.md#relationship-to-fighters-legacy)).

## `inventory/<binary>/` — Ghidra ground truth (exported, local-only)

Fact snapshots regenerated by `scripts/ghidra/export_inventory.sh <binary>`; never edited by
hand and **never committed** — Ghidra result output stays out of the repo; only the
tools/scripts that produce it are tracked ([#342](https://github.com/jomkz/fighters-codex/issues/342);
`db/inventory/` is gitignored). Each binary has its own directory (`db/inventory/FA.EXE/`,
`db/inventory/WAIL32.DLL/`, …) — VAs are unique only within a binary, so coverage is checked
per binary.

Where the export is present (any machine with the canonical project), `check_status.py`
runs the full coverage checks and verifies/regenerates the reconstruction matrix against
it. Where a binary's export is absent (CI, fresh clones), its coverage checks are skipped
with a note, and `docs/fa/reconstruction.md` is neither rewritten by `--write-matrix` nor
currency-checked by `--check` unless **every** binary's export is present — the committed
matrix remains the published record (its counts are facts), re-derived locally whenever
`db/` changes. A *partially* exported directory (e.g. `functions.csv` without
`globals.csv`) is still a hard error: that's a broken export, not a missing one.

**Verification without a committed baseline (post-#342).** Because Ghidra output is never
committed, a fresh machine has *no prior inventory to diff against* — there is no committed
"canonical" export anywhere in the repo. `export_inventory.sh ALL` **creates** the local
ground truth from the canonical `fa-re` project, and correctness is then asserted two ways
that need no baseline: `check_status.py --check` verifies `db/` coverage and the
referenced-globals rule against that fresh export, and the committed reconstruction-matrix
counts in `docs/fa/reconstruction.md` are the published facts `--write-matrix` regenerates.
Reproducibility — that `db/` *alone* rebuilds the same named project, with no hidden state
in the working project — is proven separately by `rebuild_audit.sh` (+ `rebuild_diff.py`):
it applies `db/` to a throwaway `fa-re-audit` project and diffs that fresh export against
the working project's local `db/inventory/` export. Both sides are local; it is never a diff
against a committed inventory. The run's result is the committed
[`db/reproducibility-audit.md`](reproducibility-audit.md).

| File (per `<binary>/`) | Contents |
|------|----------|
| `functions.csv` | `va,name,size` for every function in the program |
| `globals.csv` | `va,name,xref_count,subsystems,widths,indexed` for every data symbol with ≥1 code xref; `subsystems` = slugs whose functions reference it. `widths` = the operand size of every access (the evidence `tools/recover_globals.py` types from, [#455](https://github.com/jomkz/fighters-codex/issues/455)); `indexed` = reached via `[base + reg]`, i.e. an array base |
| `ranges.csv` | `slug,range,bytes,bytes_in_functions,functions` per manifest range — the code-coverage signal that exposes undiscovered code |
| `callsites.csv` | `callee_va,site_va,cleanup_bytes` — the stack cleanup each caller performs after a `CALL`. The evidence `tools/recover_signatures.py` reads to recover cdecl signatures ([#453](https://github.com/jomkz/fighters-codex/issues/453)); `-1` means the site shows no readable cleanup, which is silence, not zero |
| `unaccounted.csv` | `va,bytes,padding,defined_data,labels` — every run of **executable** bytes that lies in **no function body** ([#496](https://github.com/jomkz/fighters-codex/issues/496)). `padding` is edge alignment fill (`0xCC`/`0x90`/`0x00`) only; fill *inside* a run is code we have not read. Undefined code = `bytes - padding - defined_data`, ratcheted by `coverage-baseline.csv` |

### Why `functions.csv` is not, by itself, the denominator

`ImportFASmsHeadless.java` creates **labels**, not functions. Ghidra's auto-analysis defines a
function when it sees a `CALL` to the address — so code entered *only through a function pointer*
(window procs, thread entries, object proc-table entries, event callbacks) got its FA.SMS name and
was never disassembled. It is not a function, and a check that iterates functions cannot report a
function that does not exist as missing: **490 FA.SMS-named functions — `?MainWndproc@@YGJPAXIIJ@Z`,
`?StartGameThread@@YAKPAK@Z`, `_PLANEProc` among them — sat outside every count**, including the
"unclaimed" figure that [#482](https://github.com/jomkz/fighters-codex/issues/482) added to expose
exactly this class of blind spot.

`unaccounted.csv` is that blind spot, measured. Retiring the debt needs no new Ghidra step: adding a
`func` row to `db/symbols/` **is** the fix, because `ApplySymbols.java` disassembles the VA and
creates the function before naming it. A run whose `labels` column is non-empty is therefore free to
claim — the binary is already telling you the function's name.

For the code the binary does **not** name, `scripts/ghidra/SweepUndefinedCode.java` (run by
`bootstrap.sh` and `rebuild_audit.sh`, so the project stays reproducible) disassembles what is left
— but only where there is evidence of a function **entry**: a `CALL` to the address, or a `DATA`
reference (something *stores* the address, which is how a proc-table entry or callback is reached).
Everything else is left alone and reported. That restraint is not fastidiousness; each rule below
was written because its absence manufactured functions out of thin air:

| Rejected as evidence | Because |
|---|---|
| `8B FF` as a prologue | In this toolchain `mov edi,edi` is **inter-function NOP fill**, not a hot-patch prologue. Reading it as an entry made 90 nine-byte "functions" out of the padding, while the real entry sat just past it. |
| a `JUMP` to the address | Its target is as likely a switch case or loop head **inside** an existing function. Two such fragments (`0x00465441`, `0x004668B0`) decompiled into the verbatim tails of `_CTEval_maxspeeddiff` and `CTRestoreState`. |
| a `DATA` ref into a jump table | MSVC emits switch tables into `.text`, so a table slot satisfies "something stores this address". A run whose first two dwords are both `.text` addresses is a table, not an entry. |
| a body that never terminates | A real function returns or tail-jumps. A decoded-garbage body that merely runs on into the next function's entry is a fragment; it is created, tested, and removed. |

**A wrong function is worse than an undefined gap**: the gap is honest debt the ratchet can see,
while the fiction inflates the denominator and then reports coverage against it.

### The binary-level ratchet — `coverage-baseline.csv`

Per-subsystem coverage only looks inside the ranges a subsystem declares, so it can only ever
report 100% of what was declared — a tautology that let 49% of FA.EXE go unmeasured
([#482](https://github.com/jomkz/fighters-codex/issues/482)). `coverage-baseline.csv` anchors the
count to the **binary** instead, one row per binary with three ratcheted columns that may only
shrink (`check_binary_coverage` fails if any grows):

| Column | What it counts | Closes at |
|---|---|---|
| `unclaimed_functions` | `functions.csv` entries with no `db/symbols` row — named FA.SMS code *and* anonymous `FUN_` statics alike | — (aggregate signal) |
| `named_unclaimed` | the subset of the above whose inventory name is a **recovered** symbol (not `FUN_`/`thunk_FUN_`) — the code the binary already tells us the name of | **0 ⇒ #482 done** |
| `undefined_bytes` | executable bytes in **no** function body at all (from `unaccounted.csv`; [#496](https://github.com/jomkz/fighters-codex/issues/496)) | 0 |

`named_unclaimed` is the one that matters: "reconstruct FA.EXE" means documenting the functions the
binary *names*, and lumping them in with anonymous statics (`unclaimed_functions`) hid that debt
inside a larger total. It is the check [#482](https://github.com/jomkz/fighters-codex/issues/482)
calls "the real fix". Retiring an entry is the same one-line move as everywhere else: add its
`db/symbols` row (named or waived) and re-export, then lower the baseline to match.

## Definition of done (per subsystem, enforced at `status=complete`)

1. **Functions:** every `inventory/functions.csv` entry inside the subsystem's ranges
   (minus VAs claimed by other subsystems, plus VAs this file claims anywhere) appears
   in the symbols file, named or waived; no `FUN_*` names remain.
2. **Data:** every `inventory/globals.csv` row tagged with the subsystem is named or
   waived (the *referenced-globals rule* — zero-xref struct interiors don't count).
3. **Doc:** the manifest `doc` exists, its symbol tables agree with the DB, and it
   embeds a theme-aware SVG flow diagram (structure checked; see
   [spec-authoring.md](../docs/spec-authoring.md)).
