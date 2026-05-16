# In-Game Menu Layout (.MNU)

FA_2.LIB contains 12 `.MNU` files (e.g. `ARMPLANE.MNU`). These define the top-level screens in the FA in-game menu system — mission selection, loadout configuration, campaign management. Each is a **DOS MZ executable overlay** loaded by the FA engine at runtime.

## Format

DOS MZ executable (magic `4D 5A`). All observed `.MNU` files decompressed to **4608 bytes**.

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
| FA_2.LIB | 12 |

## TODO — Deep Dive

- Disassemble a `.MNU` overlay to identify screen layout fields and references to child `.DLG` files
- Map the 12 menu names to their in-game screens

## Related

- [DLG.md](DLG.md) — dialog box overlays nested within menus
