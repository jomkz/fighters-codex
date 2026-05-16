# Compiled AI Module (.BI)

FA_2.LIB contains 9 `.BI` files — exactly one per `.AI` script file (e.g. `AC130.BI` paired with `AC130.AI`). Each is a **DOS MZ executable overlay** loaded by the FA engine at runtime.

## Format

DOS MZ executable (magic `4D 5A`). The FA engine uses compiled overlay modules for many data-heavy formats; `.BI` files follow this same pattern.

```
Offset  Value   Description
------  -----   -----------
0x00    4D 5A   MZ magic
0x02    80 00   Last page bytes used (128)
0x04    01 00   Pages in file
0x06    00 00   Relocation entries
0x08    04 00   Header size in paragraphs (64 bytes)
...
0x3C    80 00   File offset of PE/overlay header
```

All observed `.BI` files are **4608 bytes** decompressed (DCL-compressed in the LIB).

## Location

| LIB | Count |
|-----|-------|
| FA_2.LIB | 9 |

## TODO — Deep Dive

- Disassemble a `.BI` overlay to determine what data or code it exports
- Locate the FA.EXE loader that maps `.BI` into memory and calls into it
- Confirm whether `.BI` is a compiled form of the paired `.AI` script or independent supplementary data

## Related

- [AI.md](AI.md) — plain-text AI script with the same basename
