# Campaign Definition (.CAM)

FA_2.LIB contains 6 `.CAM` files — one per built-in campaign (e.g. `BALTIC.CAM`, `EGYPT.CAM`). Pilot save files (`.P`) store the active campaign by this filename. Each is a **DOS MZ executable overlay** loaded by the FA engine at runtime.

## Format

DOS MZ executable (magic `4D 5A`). `BALTIC.CAM` decompresses to **8704 bytes** — double the 4608-byte baseline common to other overlay formats, suggesting campaign data is more substantial than a single-screen definition.

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
| FA_2.LIB | 6 |

## TODO — Deep Dive

- Disassemble a `.CAM` overlay to identify exported data (mission list, theater references, unlock conditions)
- Determine whether `.CAM` references `.MM` theater files by name internally
- Clarify relationship to `.MC` files (21 entries vs 6 campaigns)

## Related

- [PLT.md](PLT.md) — pilot save files store the active campaign `.CAM` filename
- [MM.md](MM.md) — theater/map files likely referenced from within a `.CAM`
- [MC.md](MC.md) — campaign data files, possibly one per mission set within a campaign
