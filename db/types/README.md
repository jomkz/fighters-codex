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

A `ret N` in the callee *looks* like proof, and **on its own it is not**: it pins the N **stack**
bytes, but a fastcall callee may take 1–2 further arguments in registers on top of them. It cannot
settle an arity unless the convention is already known — which is exactly what
[`tools/recover_frames.py`](#the-ret-operand-toolsrecover_framespy) supplies, below.

Any of these would have corrupted `db/` at exactly the rate hardest to notice — and whatever the
fxe generator emits downstream with it. **A wrong datatype is worse than none.**

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

## The RET operand: `tools/recover_frames.py`

The two passes above both read the **caller**. What is left after them — the #453 tail — is the set
where the caller says nothing: no decoration to read, and no visible cleanup (MSVC merged or deferred
the pop). The evidence has to come from the **callee**, and one rule survived measurement:

```
ret_imm > 0                    the callee cleans its own stack  -> callee-cleans
AND ECX/EDX never read as an input   ...and takes nothing in registers -> stdcall
=> stdcall, arity = ret_imm / 4
```

This is the composition the negative result above is missing. `ret N` alone cannot settle an arity
because a fastcall callee may take arguments in registers *on top* of the N stack bytes — so pair it
with a test that rules the register arguments out, and the RET operand becomes the whole arity.

**The register test only ever disqualifies, and that is the whole trick.** Inferring arity *from*
register use failed at 7.6–16% (table above) because an incoming register argument is
indistinguishable from a scratch use. But the converse costs nothing: a function that reads `ECX` or
`EDX` as an input *might* be taking arguments there, so its RET operand is not the whole story —
refuse it. `_GRTo2d@8` (stdcall, no register arguments, yet its prologue reads both) is not
mistyped by this rule; it is **declined**. Refusing on a maybe costs recall. Counting on a maybe
costs correctness.

Measured against the 970 signatures the decorations prove independently, on every run:

- **263 / 263 correct, 0 false positives**
- fires on **0 of 242** known `__fastcall` functions — the counterexample class

One leak was found and closed while measuring: registers must be matched on their **base register**,
so `CX` and `CL` count as touching `ECX`. A fastcall callee taking a `ushort` receives it in `CX`,
and an exact-name comparison sees no `ECX` anywhere in the function — that single leak was the rule's
only false positive (`@Reaction@12`, fastcall/3, typed as stdcall/1).

### Also rejected here — measured, and not to be retried

| approach | result |
|---|---|
| arity from the callee's `[EBP + N]` argument reads | **unavailable**: FA.EXE is built with frame-pointer omission — only 26 of 733 unsigned functions have an EBP frame at all |
| arity from Ghidra's normalized stack references (these *do* survive FPO) | **9 false positives / 221**. It undercounts: a callee that never touches its trailing argument leaves no reference to it (true arity 10, predicted 1) |
| arity from counting the caller's `PUSH`es | **74.6%** precision. MSVC stages arguments with `SUB ESP,N` + `MOV [ESP+k]`, and pushes non-argument values |
| the two above, gated on **agreeing with each other** | 0 false positives — but it fires on **6** functions in the tail-like domain. Six samples cannot establish a precision, and an unproven rule is not a rule. Left unapplied on purpose |

```
python3 tools/recover_frames.py            # report
python3 tools/recover_frames.py --write    # needs the local Ghidra export
```

It re-proves itself against the decorations on every run and **refuses to write** if the rule stops
validating.

## The per-subsystem pass: reading what each function does

The rules above are exhausted by ~1167 of the 1871 functions. The rest have no decoration, no
caller cleanup, and no callee-cleaned stack — nothing a rule can read. They were recovered the
only way left: **by reading what each function does**, subsystem by subsystem, against the prose
docs. That is judgment work, so the point is not the reading — it is what the judgment is *checked
against*.

### The binary gates every judgment

A reader of a decompile inherits its parameter error (~16%). So every proposed signature was
mechanically checked against facts the instruction stream carries, and a proposal that contradicts
them was rejected outright:

```
ret_imm  > 0  =>  the callee cleans its own stack: the stack arity is EXACTLY ret_imm/4.
ret_imm == 0  =>  the callee cleans NOTHING. Either __cdecl (any arity, nothing in registers),
                  or a __fastcall whose arguments ALL ride in registers (zero stack args).
ret_imm == -1 =>  no consistent RET: the convention is unprovable. Refused.
reg_args >= 1 =>  requires that the function actually READ ECX (and EDX for two).
```

**`ret 0` does not mean cdecl** — that was an error in the first version of this gate. A fastcall
taking only register arguments has no stack to clean either, so it also emits a plain `RET`. What
`ret 0` proves is that *no callee-cleaned stack arguments exist*, which is a weaker and different
claim.

Of 563 proposals: **511 accepted, 52 rejected by the gate** (26 with no consistent RET operand, the
rest contradicting the arity or claiming a register argument the function never reads).

### The one judgment the binary cannot settle — and what it cost

Whether a register the function reads is an **incoming argument** or **scratch**. No rule decides
this (every attempt is in the table above, at 7.6–16% error). Where an analyst claimed "scratch" for
a register the function demonstrably reads, the claim went to an **adversarial reviewer** whose brief
was to *refute* it from the raw bytes.

It refuted **3 of 11** — and one of those matters more than the other two put together:
`SetShading` (`0x4CD854`) was proposed as `void __cdecl SetShading(void)`. It is really a hand-written
routine taking a **3-component vector in EAX/EBX/ECX**. A no-argument prototype would have compiled,
propagated into the generator, and lied about the function forever.

**It also found that an evidence string can be fabricated even where the conclusion is correct** (one
cited a `rep stos` counter where the bytes show eight `push eax`). So `db/` does **not** store the
analyst's prose. Each row records what was actually *proven* — convention and stack arity, checked
against the RET operand — because a claim a later reader cannot re-derive is not evidence.

### What is honestly left

Whole classes of "function" are **not C functions and must never be signed as such**:

- **`render-core`'s ~110 `sh_op_*` handlers** are threaded-code **jump targets** dispatched through
  `vector_table` with ESI live as the bytecode cursor — several fall through into one another. They
  have no RET of their own. Declining them is the correct answer, not a gap.
- **Hand-written assembly with register conventions** C cannot express (`sincos` takes its angle in
  EBX; `isqrt16` a DX:BX pair; `_G__Texture` its arguments in EAX/EDI).
- **Variadic functions** (`sprintf`, `G_Printf`, `CallUtilProc` — whose call sites clean 4, 8 and 12
  bytes) have no fixed arity to record.

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
