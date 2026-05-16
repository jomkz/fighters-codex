# Campaign Mission Data (.MC)

FA_2.LIB contains 21 `.MC` files. With 6 `.CAM` campaigns but 21 `.MC` files, each campaign likely has 3–4 associated `.MC` files — possibly one per mission set or phase within a campaign. Each is a **DOS MZ executable overlay** loaded by the FA engine at runtime.

## Format

DOS MZ executable (magic `4D 5A`). `CATFAIL.MC` decompresses to **4608 bytes**.

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
| FA_2.LIB | 21 |

## TODO — Deep Dive

- List all 21 filenames and group by campaign prefix to confirm the 1-campaign-to-many-MC mapping
- Disassemble an `.MC` overlay to identify what campaign state it encodes (mission availability, unlock flags, scoring)
- Determine how `.CAM` overlays reference `.MC` files

## Related

- [CAM.md](CAM.md) — campaign definition files that likely reference `.MC` entries
