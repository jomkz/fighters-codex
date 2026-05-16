# Unknown -- Hangar? (.HGR)

FA_2.LIB contains 2 `.HGR` files (`H_AIRB.HGR`, one other). The name prefix `H_` and extension suggest "hangar" — possibly defining the loadout or aircraft selection hangar screen layout. Each is a **DOS MZ executable overlay** loaded by the FA engine at runtime.

## Format

DOS MZ executable (magic `4D 5A`). `H_AIRB.HGR` decompresses to **4608 bytes**.

```
Offset  Value   Description
------  -----   -----------
0x00    4D 5A   MZ magic
0x02    80 00   Last page bytes used (128)
0x04    01 00   Pages in file
...
0x3C    80 00   Overlay header offset
```

## Location

| LIB | Count |
|-----|-------|
| FA_2.LIB | 2 |

## TODO — Deep Dive

- Identify both filenames and determine purpose from names and disassembly
- Locate FA.EXE references to `.HGR` files to understand context of use

## Related

- [MNU.md](MNU.md) — in-game menu overlays, likely the parent system
