# Aircraft Type Supplement (.PTS)

FA_2.LIB contains 37 `.PTS` files (e.g. `A4E.PTS`). The extension mirrors `.PT` (145 aircraft flight model files), strongly suggesting `.PTS` stores supplementary per-aircraft data — possibly cockpit configuration, avionics suite, or sensor/ECM loadout defaults not encoded in the primary `.PT` BRF record. Each is a **DOS MZ executable overlay** loaded by the FA engine at runtime.

## Format

DOS MZ executable (magic `4D 5A`). All observed `.PTS` files decompressed to **4608 bytes**.

```
Offset  Value   Description
------  -----   -----------
0x00    4D 5A   MZ magic
0x02    80 00   Last page bytes used (128)
0x04    01 00   Pages in file
...
0x3C    80 00   Overlay header offset
```

Note: 37 `.PTS` files vs 145 `.PT` files — not every aircraft has a `.PTS` entry. The subset with `.PTS` may indicate aircraft with non-standard cockpit or sensor configurations.

## Location

| LIB | Count |
|-----|-------|
| FA_2.LIB | 37 |

## TODO — Deep Dive

- List which aircraft have `.PTS` entries and identify what distinguishes them from those without
- Disassemble a `.PTS` overlay to identify supplementary data fields
- Locate FA.EXE references to `.PTS` loading to understand when it is used relative to `.PT`

## Related

- [BRF.md](BRF.md) — `.PT` aircraft flight model records
- [HUD.md](HUD.md) — HUD definitions, possibly linked via `.PTS`
