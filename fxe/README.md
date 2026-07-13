# `fxe` — the clean-room source port of the game executable

**Status: declarations only.** There is no runtime yet — no entry point, no `fx_render` hook
([#292](https://github.com/jomkz/fighters-codex/issues/292)), no camera
([#387](https://github.com/jomkz/fighters-codex/issues/387)). What exists today is the
generated C++ *surface* of the reconstruction, and a build that compiles it.

`fxe` is the executable-level analogue of the `fx_lib` / `fx` / `fxs` validation layer. Where a
byte-identical codec proves a **format** is understood, a runnable source port proves the
**game-executable reconstruction** is. Epic
[#280](https://github.com/jomkz/fighters-codex/issues/280); legal model and clean-room
provenance in [ADR-0002](../docs/adr/0002-fxe-clean-room-source-port.md).

## The generator is the source of truth

Nothing under `generated/` is hand-written or hand-edited. It is emitted from the symbol
database by `tools/gen_fxe.py`, committed, and kept in sync by CI — the same pattern as the
generated status matrices.

```
python3 tools/gen_fxe.py --write   # regenerate after any db/ change
python3 tools/gen_fxe.py --check   # CI + ctest: fails if the committed C++ has drifted
```

**Clean-room (ADR-0002):** every declaration is derived from facts *we* recovered — `db/symbols/`,
`db/types/` — and from our own prose. Nothing is transcribed from decompiler output or from
OpenFA.

## Why compiling it is the point

`fxe/compile_check.cpp` includes the whole generated tree, and the build compiles it under the
project's warning bar. That is not busywork:

- **A declaration that is not valid C++ is not a reconstruction of anything.** The very first
  generated tree kept the MSVC calling convention inside function-pointer parameters — which gcc
  and clang reject outright. The compile check caught it immediately.
- **The recovered sizes become claims the compiler checks.** `static_assert(sizeof(CN_INFO) ==
  0xDDC)` passing means the one struct layout `db/types/` maps really is the config body
  `CN_ReadConfig` reads. A layout that drifts stops the build instead of silently miscompiling
  every access the port makes through it.

## What it refuses to invent

The reconstruction is incomplete, and the generated tree **says so in the source** rather than
papering over it:

| in `db/` | emitted |
|---|---|
| signature recovered | a real declaration |
| signature **not** recovered | `// TODO(#453): 0x… name — signature not recovered` |
| global type recovered | `extern <type> name;  // 0x…` |
| global type **not** recovered | `// TODO(#455): 0x… name — type not recovered` |

A guessed `void f()` would compile and then *lie* about what the original function took. A TODO
cannot. The generated headers therefore map what the reconstruction owes **of the code `db/`
carries**: **1687 of 1734 C functions declared (137 more are classified as not-C, see #479), and
173 of 176 globals typed.**

That is not the whole debt, and the header must not pretend otherwise: **1312 of FA.EXE's 3047
functions are not in `db/` at all** ([#482](https://github.com/jomkz/fighters-codex/issues/482)),
so they cannot appear here even as a TODO. The generated tree is honest about what it knows;
`db/` is what still has to grow.

`undefinedN` is preserved verbatim wherever it appears. It means "N bytes, type not recovered"
([#452](https://github.com/jomkz/fighters-codex/issues/452),
[#455](https://github.com/jomkz/fighters-codex/issues/455)); flattening it into `int` or `u32`
would invent semantics the evidence never proved — and 12 of the first 32 typed globals turned
out to be pointers.

Calling conventions are dropped from the emitted code and kept as provenance comments: they are
an artifact of the 32-bit x86 ABI, and `fxe` is a modern 64-bit port. They remain a recovered
fact in `db/`, which is where they belong.

## Layout

```
fxe/
  generated/
    fa_types.hpp            # scalars, the recovered type vocabulary, asserted sizes
    fxe.hpp                 # includes everything — the header the build compiles
    subsystems/<slug>.hpp   # one per subsystem: its globals, its functions, its TODOs
  compile_check.cpp         # the translation unit that makes the compiler evaluate it all
```
