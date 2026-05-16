# Heads-Up Display (.HUD)

FA_2.LIB contains 46 `.HUD` files — one per aircraft type (e.g. `A7.HUD`, `F22.HUD`). Each defines the cockpit HUD layout for that aircraft. Each is a **DOS MZ executable overlay** loaded by the FA engine at runtime.

## Format

DOS MZ executable (magic `4D 5A`). All observed `.HUD` files decompressed to **4608 bytes**.

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
| FA_2.LIB | 46 |

## TODO — Deep Dive

- Disassemble a `.HUD` overlay to identify element layout (gauge positions, indicator types, scale factors)
- Determine how `.PT` aircraft records reference the paired `.HUD` by name
- Map the overlay data section structure (likely a table of screen-space coordinates and element IDs)

## Related

- [BRF.md](BRF.md) — `.PT` aircraft type records that likely reference the corresponding `.HUD`
- [FNT.md](FNT.md) — font files used to render HUD text elements
- [PIC.md](PIC.md) — bitmap assets used for HUD graphical elements
