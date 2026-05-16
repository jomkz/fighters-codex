# Unknown Binary (.BIN)

FA_2.LIB contains 6 `.BIN` files. The only known file by name is `INSIGMAP.BIN` (256 bytes), which appears to be a **lookup table** — a flat 256-entry byte array likely mapping insignia slot indices to palette or asset IDs.

## Format

From `INSIGMAP.BIN` (256 bytes):

```
Offset  Value   Description
------  -----   -----------
0x00    00      First entry (slot 0 = none/empty?)
0x01–FF 3B ...  Remaining 255 entries, all 0x3B (59 decimal)
```

The uniform `0x3B` fill suggests most slots are unassigned placeholders. The other 5 `.BIN` files may follow a different structure or use the same pattern.

## Location

| LIB | Count |
|-----|-------|
| FA_2.LIB | 6 |

## TODO — Deep Dive

- Identify all 6 filenames and whether they share the 256-byte table structure
- Determine what `0x3B` signifies as a default/sentinel value
- Cross-reference `INSIGMAP.BIN` with insignia-related `.PIC` assets and pilot save file insignia fields ([PLT.md](PLT.md))
