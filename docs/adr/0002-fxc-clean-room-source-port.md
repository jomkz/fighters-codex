# ADR-0002: fxc — a committed, generated, clean-room source port of the game executable

**Status:** Accepted (2026-07-04)
**Serves:** [epic #280](https://github.com/jomkz/fighters-codex/issues/280) (fxc),
[epic #281](https://github.com/jomkz/fighters-codex/issues/281) (fx_render); charters the
executable-level validation program alongside the format-validation layer.

## Context

The two reconstruction programs are complete: the game executable (all 20 subsystems named and
documented, [#209](https://github.com/jomkz/fighters-codex/issues/209)) and its overlay binaries
([#247](https://github.com/jomkz/fighters-codex/issues/247)). The
[`db/`](https://github.com/jomkz/fighters-codex/blob/main/db/README.md) symbol
database — struct maps with
offsets, function signatures, dispatch tables, a globals registry — plus the prose subsystem docs now
describe the executable's behaviour completely enough to *build against*.

The format side of the toolkit has a validation principle: a byte-identical codec is the proof a
format is understood. The executable side had no equivalent. **fxc** is that equivalent — a runnable,
clean-room, modern C++ **source port**: give it the content from the user's original disks and it
plays the game, on modern rendering and audio. A port that boots real content and behaves correctly
is proof-of-understanding at the executable level.

Two questions shaped the decision:

- **Legal posture.** An open-source re-implementation of a commercial game engine is only safe as a
  *clean-room* work — written from documentation of behaviour and non-copyrightable facts
  (interfaces, struct layouts, algorithms described in prose), never a transcription of the original's
  decompiled expression. Separately, the game's **content** is EA's; the port must ship none of it.
- **Where the code lives.** Keeping generated output out of the repo was considered as caution, but
  that reduces *visibility*, not *liability* — and it forfeits a browsable, buildable, CI-tested port.

## Options considered

1. **Generated-only, not committed** — the repo holds only the generator; fxc is ephemeral build
   output. Lowest visibility, but no usable artifact and no CI coverage of the port itself.
2. **Committed, generator is the source of truth** *(chosen)* — a committed generator emits fxc's C++;
   the emitted source is committed and kept in sync by CI (the same pattern already used for the
   generated matrices). Usable port + documented legal posture.
3. **Committed, hand-written** — a conventional hand-written clean-room port, no generator. Simplest to
   start and unambiguously clean-room, but loses the generate-from-truth link to `db/` + docs and
   drifts by hand.

## Decision

**fxc is a committed, generated, clean-room modern C++ source port of the game executable, with the
generator as the source of truth.**

- **Generated from `db/` + docs** by an in-repo generator; the emitted C++ is committed and a CI
  currency check regenerates and diffs it (as the reconstruction/format matrices are checked).
- **Clean-room discipline:** the generator consumes *our own* facts and prose only. Behaviour is
  expressed independently; decompiler output is **never** transcribed. Same boundary as the rest of
  the RE effort (CLAUDE.md).
- **Source-port legal model:** fxc ships **no assets** and is inert without the user's original disks
  (the ScummVM / OpenMW model). The [NOTICE](https://github.com/jomkz/fighters-codex/blob/main/NOTICE)
  records the clean-room provenance and the require-original-content posture.
- **Rendering via `fx_render`** (ADR forthcoming if it grows): the OpenGL + faithful-software
  backends are a shared MIT module, extracted from fx-gui, that fxc and fx-gui both use — one
  renderer, not three (the fighters-legacy engine may adopt it too).
- **Validation lens, independent of fa-bridge:** fxc proves the reconstruction docs by running; it is
  not a fa-bridge dependency. fxc (MIT source port) and fa-bridge (GPL bridge into the
  fighters-legacy engine) are independent consumers of the same reconstruction and share no code.
- **Not a 1.0 gate:** fxc is a stretch program on its own milestone, interleaved with the Phase 4
  validation train.

Naming: `fxc` fits the fx family (fx_lib / fx / fx-gui / fxc) and reads as "fx-compiler" — the
generator compiles `db/` + docs into C++. It is unrelated to Microsoft's `fxc.exe` HLSL compiler.

## Consequences

- New milestone (**fxc — Clean-room source port**) and epics #280 (fxc) / #281 (fx_render); #279
  scopes the fx_lib asset interpreters the renderer consumes.
- CI gains an fxc currency check (regenerate → diff); the build never commits assets.
- MIT→GPL reuse is one-way and clean: fa-bridge / fighters-legacy may consume `fx_lib` and
  `fx_render`; nothing flows back.
- The repo now hosts a generated *implementation* of the executable (fxc), not only documentation —
  a deliberate extension of "the docs are the product": fxc is the docs made executable.
