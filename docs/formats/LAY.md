# Cloud Layer (.LAY)

FA_2.LIB contains 24 `.LAY` files (e.g. `CLOUD1.LAY`, `DAY2.LAY`). Each defines a cloud/atmosphere layer configuration used during flight rendering. Referenced by name from `.MM` theater files (`layer day2.LAY 0`). Each is a **DOS MZ executable overlay** loaded by the FA engine at runtime.

## Format

DOS MZ executable (magic `4D 5A`). `CLOUD1.LAY` decompresses to **16896 bytes** (0x4200) — the largest of the common overlay sizes, suggesting substantial layer data (possibly multiple altitude bands or particle definitions).

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
| FA_2.LIB | 24 |

## TODO — Deep Dive

- Disassemble a `.LAY` overlay to identify cloud band definitions (altitude, density, color)
- Map the reference from `.MM` (`layer <name>.LAY <index>`) to how the engine selects layers at runtime

## Related

- [MM.md](MM.md) — theater files that reference `.LAY` files by name
