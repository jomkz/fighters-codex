# In-Game Menu Dialog Layout (.DLG)

FA_2.LIB contains 92 `.DLG` files. These define individual dialog boxes in the FA in-game menu system. Each is a **DOS MZ executable overlay** loaded by the FA engine at runtime.

## Format

DOS MZ executable (magic `4D 5A`). All observed `.DLG` files decompressed to **4608 bytes**.

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
| FA_2.LIB | 92 |

## TODO — Deep Dive

- Disassemble a `.DLG` overlay to identify layout fields (control positions, labels, button definitions)
- Determine how `.DLG` files are referenced from `.MNU` menus
- Map the overlay entry point and data section layout

## Related

- [MNU.md](MNU.md) — top-level menu layouts that reference dialog boxes
