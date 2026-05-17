# Sky / Cloud / Ocean Layer (.LAY)

FA_2.LIB contains 24 `.LAY` files (e.g. `CLOUD1.LAY`, `DAY1.LAY`, `DAY2.LAY`). Each defines a complete atmospheric rendering configuration — sky gradient, cloud layers, horizon, and ocean surface — used during flight. Referenced by name from `.MM` theater files (`layer day2.LAY 0`). Each is a **Win32 PE DLL** loaded at runtime via `LoadLibrary`.

## Format

Win32 PE DLL. `CLOUD1.LAY` decompresses to **16896 bytes** — significantly larger than other overlays (4608 bytes), because `.LAY` files embed the full sky/atmosphere rendering lookup tables as data. `DAY1.LAY` decompresses to **20992 bytes**, confirming that file size is not uniform across LAY variants.

## Content

String analysis and direct CODE section analysis of `CLOUD1.LAY` and `DAY1.LAY` reveals:

- **`wave1.SH`** — animated ocean wave mesh, referenced by name in both cloud and day variants (string at CODE VA ~0x11C2)
- **`_T_HorizonProc`** — **imported** from `main.dll` (not exported). LAY files call the engine's horizon rendering function rather than implementing it.
- **Pointer table** — CODE section starts at VA 0x1000 with an array of u32 VAs pointing to sub-blocks within the same CODE section
- **Sky gradient tables** — byte arrays in CODE with values 0x09–0x3F (CLOUD1) or 0x01–0x3C (DAY1), representing sky brightness/color indices
- **Wave parameter block** — 16 bytes at CODE VA ~0x11B0: fixed-point ocean wave amplitude/frequency parameters (identical between CLOUD1 and DAY1 — not weather-specific)
- **Layer constants** — small integers and u32 counts scattered through the first 0x80 bytes of CODE

Note: the previous claim that `_T_HorizonProc` is an export was incorrect — it is an import in all observed LAY files.

## Location

| LIB | Count |
|-----|-------|
| FA_2.LIB | 24 |

## CODE Section Layout (Partially Confirmed)

LAY files use **Phar Lap PE format** (signature `PL\0\0`). The CODE section (13,824 bytes in CLOUD1.LAY) contains all rendering data. The engine interprets this data via the `_T_HorizonProc` call.

### CODE section structure (VA offsets from 0x1000)

| VA range | Content |
|----------|---------|
| 0x1000–0x006F | Pointer table — u32 VAs to sub-blocks; includes small integer constants (7, 6) interspersed |
| 0x1070–0x00AF | Layer parameters — u32 counts, INT_MAX sentinel, small integers |
| 0x10B0–0x1176 | **Sky gradient table** — byte array, values 0x09–0x3F; encodes sky brightness/color by altitude band |
| 0x11B0–0x11BF | Wave parameters — 16 bytes of fixed-point ocean wave amplitude/frequency (same in all variants) |
| ~0x11C0 | String: `"wave1.SH"` — ocean wave mesh name |
| 0x11C0+ | Additional sub-tables (cloud density, dither patterns) |

### Sky gradient table structure (0x10B0)

The gradient table begins at CODE VA 0x10B0 with an 8-byte header: `31 00 00 00 00 00 10 10`. The first `u32` (`0x31` = 49) is likely the entry count or table ID; the trailing `10 10` may encode row stride or channel count. Gradient byte data follows immediately.

At least two sub-tables are visible: one at 0x10B0 and a second at 0x1110. Values are **non-monotonic** (range 14–63), not a simple descending ramp — the table encodes a sky color curve, not a simple linear gradient.

### CLOUD1 vs DAY1 comparison

The sky gradient table at 0x10B0 differs between variants:
- **CLOUD1**: values in the 14–63 range across multiple sub-tables, non-monotonic pattern
- **DAY1**: different value distribution in the same table positions (DAY1 is 20992 bytes vs CLOUD1's 16896 — extra data suggests additional sub-tables or a larger gradient array)

The wave parameters at 0x11B0 are **identical** in both — weather condition does not affect wave motion.

### CLOUD1B is identical to CLOUD1

Binary diff of `CLOUD1B.LAY` vs `CLOUD1.LAY` shows **zero differences** — the files are byte-for-byte copies. The `B` suffix does **not** encode a brightness variant for this file. Its purpose (alias, duplicate, or reserved slot) is unknown.

## Toolkit Roadmap

Sub-table structure and gradient header are partially confirmed. Blocked on exact row stride and sub-block type map before JSON/PNG export can produce meaningful output:

- New `lib/src/lay.cpp` + `lib/include/ft/lay.h` — parse pointer table, gradient byte arrays, wave params
- New `cli/cmd_lay.cpp`:
  - `ft lay dump <file.LAY>` — exports atmosphere parameters as JSON
  - `ft lay gradient <file.LAY> -o gradient.png` — renders sky colour ramp as a PNG strip (one pixel per step)

## TODO — Deep Dive

- Decode gradient table header `31 00 00 00 00 00 10 10` — confirm `0x31` as entry count and `10 10` as stride/channels
- Identify all sub-tables in the pointer table at 0x1000 (cloud density, dither, fog table suspected)
- Explain DAY1.LAY size difference (20992 vs 16896) — likely an additional sub-table absent from CLOUD1
- Explain CLOUD1B = CLOUD1 (byte-for-byte identical — alias, stub, or reserved slot?)
- Map the `layer <name>.LAY <index>` slot index from `.MM` files to rendering layers
- Document CLOUD / DAY / other prefix naming convention

## Related

- [MM.md](MM.md) — theater files that reference `.LAY` files via the `layer` keyword
- [SH.md](SH.md) — `wave1.SH` is the ocean wave mesh loaded by `.LAY`
- [PIC.md](PIC.md) — `ocean*06.PIC` ocean texture atlas
