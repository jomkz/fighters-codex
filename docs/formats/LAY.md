# Sky / Cloud / Ocean Layer (.LAY)

FA_2.LIB contains 24 `.LAY` files (e.g. `CLOUD1.LAY`, `DAY1.LAY`, `DAY2.LAY`). Each defines a complete atmospheric rendering configuration — sky gradient, cloud layers, horizon, and ocean surface — used during flight. Referenced by name from `.MM` theater files (`layer day2.LAY 0`). Each is a **Win32 PE DLL** loaded at runtime via `LoadLibrary`.

## Format

Win32 PE DLL. `CLOUD1.LAY` decompresses to **16896 bytes** — significantly larger than other overlays (4608 bytes), because `.LAY` files embed the full sky/atmosphere rendering lookup tables as data.

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

### CLOUD1 vs DAY1 comparison

The sky gradient table at 0x10B0 differs substantially:
- **CLOUD1**: values range from 0x3F (bright) to 0x09 (dark), smooth descending ramp → overcast/hazy sky
- **DAY1**: mostly 0x01 (flat, very dim), with a few 0x3C/0x32 peaks → clear blue sky with minimal scattering

The wave parameters at 0x11B0 are **identical** in both — weather condition does not affect wave motion.

### RE next steps

1. Trace the pointer table at 0x1000 — each entry is a VA to a named sub-block; identify sub-block types by their first byte or a preceding length field.
2. Load CLOUD1.LAY in Ghidra, add `_T_HorizonProc` label, and check whether the engine indexes into the gradient table by row (altitude) or by angle.
3. Diff CLOUD1B.LAY against CLOUD1.LAY — the `B` variant likely has the same structure but different brightness levels (dusk/dawn?).

## Toolkit Roadmap

Once the gradient table layout and sub-block structure are confirmed:

- New `lib/src/lay.cpp` + `lib/include/ft/lay.h` — parse pointer table, gradient byte arrays, wave params
- New `cli/cmd_lay.cpp`:
  - `ft lay dump <file.LAY>` — exports atmosphere parameters as JSON
  - `ft lay gradient <file.LAY> -o gradient.png` — renders sky colour ramp as a PNG strip

## TODO — Deep Dive

- Confirm the exact size and row-stride of the sky gradient table (is it 1 byte per altitude step, or multiple channels?)
- Identify remaining sub-tables pointed to by the pointer table (cloud density, dither, fog)
- Map the `layer <name>.LAY <index>` slot index from `.MM` files to rendering layers
- Document CLOUD/DAY/other prefix naming convention

## Toolkit Roadmap

Once the gradient table layout and atmosphere constants are confirmed:

- New `lib/src/lay.cpp` + `lib/include/ft/lay.h` — parse atmosphere parameter block and gradient table
- New `cli/cmd_lay.cpp`:
  - `ft lay dump <file.LAY>` — exports atmosphere parameters as JSON
  - `ft lay gradient <file.LAY> -o gradient.png` — renders the sky colour ramp as a horizontal PNG strip (one pixel per step)

## TODO — Deep Dive

- Confirm number and layout of sub-tables within the 16,896-byte data section
- Identify atmosphere constant fields (fog density, cloud altitude, dither table, horizon offset)
- Map the `layer <name>.LAY <index>` slot index from `.MM` files to rendering layers (background sky, mid cloud, foreground haze)
- Document the CLOUD / DAY / other prefix naming convention

## Related

- [MM.md](MM.md) — theater files that reference `.LAY` files via the `layer` keyword
- [SH.md](SH.md) — `wave1.SH` is the ocean wave mesh loaded by `.LAY`
- [PIC.md](PIC.md) — `ocean*06.PIC` ocean texture atlas
