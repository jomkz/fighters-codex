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
- **func rows** — an optional signature override; usually left empty because Ghidra
  already demangles the FA.SMS name into a full signature.
- **waiver rows** — always empty (a waiver is bookkeeping, not a typed symbol);
  `check_status.py` enforces this.

A type must name something `ApplyTypes` can resolve: a builtin, a `db/types/` typedef, or
a struct declared there. Empty = untyped (the default for most rows).

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
