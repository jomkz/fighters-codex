# Sky / Cloud / Ocean Layer (.LAY)

FA_2.LIB contains 24 `.LAY` files (e.g. `CLOUD1.LAY`, `DAY1.LAY`, `DAY2.LAY`). Each defines a complete atmospheric rendering configuration — sky gradient, cloud layers, horizon, and ocean surface — used during flight. Referenced by name from `.MM` theater files (`layer day2.LAY 0`). Each is a **Win32 PE DLL** loaded at runtime via `LoadLibrary`.

## Format

Win32 PE DLL. `CLOUD1.LAY` decompresses to **16896 bytes** — significantly larger than other overlays (4608 bytes), because `.LAY` files embed the full sky/atmosphere rendering lookup tables as data.

## Content

String analysis of `CLOUD1.LAY` and `DAY1.LAY` reveals:

- **`wave1.SH`** — animated ocean wave mesh, referenced by name in both cloud and day variants
- **`ocean*06.PIC`** — ocean surface texture (PIC atlas reference)
- **`_T_HorizonProc`** — exported function; implements the horizon/sky rendering procedure called by the engine's flight renderer

The large data section contains encoded color gradient tables — the garbled byte sequences are pre-computed sky color ramps, cloud density profiles, and dithering tables used for atmosphere rendering at different altitudes and times of day.

## Location

| LIB | Count |
|-----|-------|
| FA_2.LIB | 24 |

## TODO — Deep Dive

- Disassemble `_T_HorizonProc` to understand the sky gradient and cloud rendering algorithm
- Map the `layer <name>.LAY <index>` reference in `.MM` files to the engine's layer slot selection
- Identify the naming convention (CLOUD vs DAY vs other prefixes) and what each group renders

## Related

- [MM.md](MM.md) — theater files that reference `.LAY` files via the `layer` keyword
- [SH.md](SH.md) — `wave1.SH` is the ocean wave mesh loaded by `.LAY`
- [PIC.md](PIC.md) — `ocean*06.PIC` ocean texture atlas
