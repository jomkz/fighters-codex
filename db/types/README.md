# `db/types/` — recovered datatypes

C headers holding the recovered **datatypes** for the FA.EXE reconstruction program
(epic #209, typing pass #230). Where `db/symbols/` records *what a location is called*,
`db/types/` records *what shape it has* — the struct layouts and the `type` column that
together let the eventual `fc` clean-room codegen emit real C++ declarations.

## Files

- **`fa_types.h`** — the FA.EXE datatypes: scalar aliases, the struct **type vocabulary**
  (every struct named in an FA.SMS mangled signature), and the field layouts recovered so
  far. Parsed into the Ghidra project by
  [`scripts/ghidra/ApplyTypes.java`](https://github.com/jomkz/fighters-codex/blob/main/scripts/ghidra/ApplyTypes.java).

## The `type` column

`db/symbols/*.csv` carries an eighth column, `type`, holding a C type string:

- **data rows** — the symbol's C type, e.g. `CN_INFO *`, `u32`, `entity *`, `char[16]`.
- **func rows** — the function's full signature, e.g.
  `char __fastcall COLFlatGround(long, F24_POINT3 *, F24_POINT3 *, F24_POINT3 *)`.
- **waiver rows** — always empty (a waiver is bookkeeping, not a typed symbol);
  `check_status.py` enforces this.

A type must name something `ApplyTypes` can resolve: a builtin, a `db/types/` typedef, or
a struct declared there — `check_status.py --check` enforces that too, so a typo'd type
fails CI rather than silently vanishing from whatever the fxe generator emits.

## Signatures: `tools/gen_signatures.py`

This column used to be left empty for functions, on the reasoning that *"Ghidra already
demangles the FA.SMS name into a full signature."* It does — but only **inside the Ghidra
project**, where neither the [fxe generator](https://github.com/jomkz/fighters-codex/issues/280)
nor CI (which has no Ghidra) can see it. So the signatures are materialized into `db/`
instead, where they are reviewable, diffable facts:

```
python3 tools/gen_signatures.py --write   # fill the column
python3 tools/gen_signatures.py --check   # CI + ctest: currency and agreement
```

The MSVC decoration on an FA.SMS name carries two different amounts of truth:

| Decoration | Example | What it proves |
|---|---|---|
| C++ mangling | `?COLFlatGround@@YIDJPAUF24_POINT3@@00@Z` | the **complete** signature — convention, return type, parameter types |
| C, with `@N` | `_FMFuelConsumption@4`, `@FMBurnNPCFuel@4` | calling convention and argument **byte count** — no types |
| C, no `@N` | `_CTVarDiff` | a convention, and nothing more |

Every `@N` in the corpus is a multiple of 4: the arguments are all 4-byte dwords, so `N/4`
is the arity exactly. Those functions get `undefined4` parameters — "4 bytes, type not
recovered" — because that is what the name proves. Inventing a plausible type there would
violate the rule this file is built on: **a wrong datatype is worse than none.** A cdecl
name with no `@N` proves no arity at all, and a C prototype cannot say "unknown number of
arguments" without lying about the count, so it yields nothing and waits for the
per-subsystem recovery in
[#453](https://github.com/jomkz/fighters-codex/issues/453).

**The derivation is a floor, not a ceiling.** Recovery is *expected* to sharpen these — an
`undefined4` becoming a real `entity *` as the type is understood. `--check` therefore does
not demand byte-equality with the derivation. It enforces only the part the name actually
proves:

> a stored signature may sharpen a type, but it may never contradict the calling convention
> or the argument count the decoration establishes.

A hand-written signature that disagrees with the decoration is not a stylistic difference —
it is a bug in the recovery, and that check is how it gets caught.

The C++-mangled names also spell out their parameter *types*, which is where the second
block of the type vocabulary in `fa_types.h` came from: `F24_POINT3`, `FVERTEX`, `TVERTEX`,
`T_BITMAP`, `NET_PKT`, `sockaddr_ipx`, `JOYRESULT`, and two dozen more — the original
developers' own type names, recovered from the symbols rather than guessed.

### What Ghidra cannot swallow (13 of 685 on FA.EXE)

`db/` records the recovered signature; `ApplyTypes` is only a *consumer* of it, and its
parsers have limits `db/` does not. These rows apply cleanly everywhere else — the fxe
generator reads `db/`, not the Ghidra project — but `apply_types.sh` reports them as
`TYPE FAIL` and moves on:

| Cause | Rows | Why |
|---|---|---|
| function-pointer parameters | 3 | `FunctionSignatureParser` cannot parse a `void (__cdecl *)(…)` argument |
| the `const` qualifier | 1 | `DataTypeParser` will not resolve `const void *` |
| MSVC CRT types | 9 | `EHRegistrationNode`, `EHExceptionRecord`, `_EXCEPTION_POINTERS`, `_s_FuncInfo` collide with Ghidra's own built-in Win32/CRT definitions |

None of these is a bad signature: all 13 were cross-checked against `llvm-undname`, and the
types declared alongside them in the same header block (`F24_POINT3`, `T_HANDLE`, `MODSPEC`)
resolve fine. Do not "fix" them by weakening what `db/` records — a Ghidra parser limitation
is not a reason to throw away a recovered fact.

## Signatures the name does not encode: `tools/recover_signatures.py`

The rows whose names encode nothing — the cdecl names with no arity (`_CTVarDiff`) and the names
we coined ourselves (`CTLoadProgram`) — must get their signature from the **code**
([#453](https://github.com/jomkz/fighters-codex/issues/453)).

```
python3 tools/recover_signatures.py --write   # needs the local Ghidra export
python3 tools/recover_signatures.py --check   # no-ops where db/inventory/ is absent (CI)
```

**One rule, because only one survives.** A cdecl caller pops its own arguments right after the
`CALL` (`ADD ESP, N`, or `POP ECX` for a single dword — `callsites.csv` in the inventory export
records it). Two things make that byte count trustworthy:

1. Seeing the *caller* clean up **proves** the convention is caller-cleans, i.e. cdecl — a
   stdcall or fastcall callee cleans up after itself, and its callers never would.
2. cdecl passes **no** arguments in registers, so the cleanup is the **full** arity. There is
   nothing hidden that we could be undercounting.

Validated against the functions whose true arity the C++ mangling gives us independently:
**75 correct, 0 wrong.**

### The negative result — don't re-litigate this without new evidence

Every other route was built, measured against known-arity functions, and **rejected**. Each has
to guess whether arguments arrived in `ECX`/`EDX`, and none can:

| approach | error rate |
|---|---|
| caller-side `ECX`/`EDX`, unanimous across sites | **7.6%** — undercounts: the value was already in `ECX`, so no load is emitted |
| caller-side `ECX`/`EDX`, max across sites | **15%** — overcounts: `EDX` is a scratch register (43 of `@MMFreePtr@4`'s 81 callers write it) |
| callee-side "reads `ECX` before writing it" | **11.6%** — `_GRTo2d@8` is stdcall with no register arguments, yet its prologue reads both |
| Ghidra's own decompiler parameter list | **~16%** — `MPBuildSpawnPayload` pops 20 bytes (5 arguments); Ghidra says 2 |

A `ret N` in the callee *looks* like proof, and is not: it pins the N **stack** bytes, but a
fastcall callee may take 1–2 further arguments in registers on top of them. It cannot settle an
arity unless the convention is already known.

Any of these would have corrupted `db/` at exactly the rate hardest to notice — and whatever the
fxe generator emits downstream with it. **A wrong datatype is worse than none.** The remaining
~900 rows are left to the per-subsystem, docs-corroborated pass #453 describes.

### The corroboration gate

Strictness scales with what is actually being inferred:

- **The name proves cdecl** (`_name`): we are only reading the *arity* off the cleanup, so one
  unanimous call site is enough.
- **The name proves nothing** (a name we coined): the cleanup must establish the *convention*
  too, and a lone `ADD ESP` after a `CALL` can be a stack-frame teardown or a discarded
  temporary. So it needs **≥2 call sites, with cleanup visible at a majority of them**. Ungated,
  this misread 8 known callee-cleans functions as cdecl — `@PutCurObj@0` shows cleanup at *1 of
  its 108* call sites. The gate admits **0** false positives across all 673 known callee-cleans
  functions.
- **The name proves a callee-cleans convention** (`_name@N`, `@name@N`): the decoration is proof
  and the call sites get no vote — there, the cleanup evidence is the weaker witness.

`--check` re-proves the rule against the known-arity functions on every run: if a Ghidra upgrade
ever perturbs the evidence, the rule's accuracy must be re-demonstrated, not assumed.

## Typing the globals: `tools/recover_globals.py`

`db/symbols/*.csv` carries thousands of `data` rows, and 32 of them had a type. The fxe
generator is meant to emit a **typed** globals registry; an untyped one is a list of addresses.
[#455](https://github.com/jomkz/fighters-codex/issues/455).

```
python3 tools/recover_globals.py --write   # needs the local Ghidra export
python3 tools/recover_globals.py --check   # no-ops where db/inventory/ is absent (CI)
```

**The evidence is the instruction itself.** An operand's size *proves* the access width:
`MOV AL,[x]` touches one byte, `MOV AX,[x]` two, `MOV EAX,[x]` four. `ExportInventory` records
it per address (`globals.csv` → `widths`, `indexed`). Validated against the 32 globals typed by
hand: **24 comparable, 24 agree, 0 mismatches.**

**But width proves the SIZE, not the SEMANTICS.** A 4-byte global is equally consistent with a
`u32` counter and a `T_HANDLE *` — and **12 of those first 32 globals turned out to be
pointers**. So a width of 4 becomes **`undefined4`**, the same honest idiom #452 uses for the
arguments an `@N` decoration counts but does not describe. Guessing `u32` would flatten every
pointer into an integer. Sharpening an `undefined4` into a real `entity *` as the type is
recovered is expected and allowed.

### What is refused, and why

| refused | why |
|---|---|
| **indexed** accesses (`[base + reg]`) | the address is an **array base** — its width is the *element* width, so typing it as a scalar hands the port an object of the wrong size |
| **conflicting widths** | an address touched as both a byte and a dword is not a plain scalar; that is a finding for #454, not something to average |
| **no width evidence** | the address is only ever *taken*, never dereferenced directly |
| **inside a function body** | a `data` row whose address lies within a function is a **code label**, not a global — MSVC's `__NLG_Return2` is a branch target in the middle of code. Ghidra refuses to lay data over instructions, and it is right to: typing it would put a fictional variable into the generator's registry |

`--check` re-proves the rule on every run: a stored type whose size contradicts the observed
access width is an error, not a preference.

Struct *layouts* are #454's job. This column types an **address** — interiors of an aggregate
are already separate rows under the referenced-globals rule (the "interior of X" waivers).

## Why this is conservative

The struct field maps in [`docs/fa/structs.md`](../../docs/fa/structs.md) are
**access-pattern annotations** — `RecoverStructs.java` records every `[reg + constant]`
dereference in a scan range. They overlap (a dword read and a byte read of the same word
both appear), their sizes are inferred from the next observed offset, and some inferred
tails do not close to the known total size. They are invaluable as a field *census* but are
**not byte-exact layouts**. `fa_types.h` therefore names only fields we are confident are
correct and leaves every unmapped or contradictory region as explicit `reserved` padding —
a wrong datatype is worse than none. Interiors are filled in per-subsystem as they are
recovered on the bench; the type *names* are declared up front so pointers type-check today.

## Workflow

```
edit db/types/*.h and the db/symbols type column
  -> scripts/ghidra/apply_symbols.sh   (names)
  -> scripts/ghidra/apply_types.sh     (datatypes + the type column)
  -> scripts/ghidra/export_inventory.sh
  -> python3 tools/check_status.py --check
```

Same canonical-project rule as `db/README.md`: types are applied to and exported from the
Fedora `fa-re` project; a bench copy is never the source of truth.

**Per-binary headers:** the top-level `db/types/*.h` (e.g. `fa_types.h`) are FA.EXE's. An
overlay/companion binary's recovered struct layouts go in `db/types/<binary>/*.h` (e.g.
`db/types/WAIL32.DLL/`); `apply_types.sh <binary>` parses the top-level headers plus that
binary's subdirectory and applies only that binary's `type` column.
