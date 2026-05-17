# Sky / Cloud / Ocean Layer (.LAY)

FA_2.LIB contains 24 `.LAY` files (e.g. `CLOUD1.LAY`, `DAY1.LAY`, `DAY2.LAY`). Each defines a complete atmospheric rendering configuration — sky gradient, cloud layers, horizon, and ocean surface — used during flight. Referenced by name from `.MM` theater files (`layer day2.LAY 0`). Each is a **Win32 PE DLL** loaded at runtime via `LoadLibrary`.

## Format

Win32 PE DLL. `CLOUD1.LAY` decompresses to **16896 bytes** — significantly larger than other overlays (4608 bytes), because `.LAY` files embed the full sky/atmosphere rendering lookup tables as data. `DAY1.LAY` decompresses to **20992 bytes**, confirming that file size is not uniform across LAY variants.

## Content

String analysis, CODE section analysis, and FA.EXE decompilation reveal:

- **`wave1.SH`** — animated ocean wave mesh, referenced by name in both cloud and day variants (string at CODE VA ~0x11C2)
- **`_T_HorizonProc`** — **imported** from `main.dll` (not exported). LAY files call the engine's horizon rendering function rather than implementing it.
- **DLL data header** — first 0x78 bytes (0x1e dwords) of the CODE section are copied verbatim to FA.EXE globals at load time. Contains: count/parameter fields, and VA pointers to the active-layer struct, the colour-matching table, the palette buffer, and the LAYER array.
- **LAYER array** — `N` entries × 0x160 bytes each, at a CODE VA stored in the header. Each entry defines one sky condition (altitude range, gradient table, cloud textures, visibility).
- **Sky gradient tables** — byte arrays in CODE, values 0x09–0x3F (CLOUD1) or 0x01–0x3C (DAY1). These are copied directly into LAYER entries at load time.
- **Wave parameter block** — 16 bytes at CODE VA ~0x11B0: fixed-point ocean wave amplitude/frequency parameters (identical between CLOUD1 and DAY1 — not weather-specific)
- **Colour palette** — 0xc0 (192) dwords at a VA in the header; copied to a working buffer used by the gradient blending pipeline.
- **Colour entry table** — 48-byte entries (stride 0x30); each entry `[terminator_byte][R][G][B][…]`. Used by `FindNearestColorEntry` to find the nearest-matching palette entry for each LAYER's base colour.

Note: the previous claim that `_T_HorizonProc` is an export was incorrect — it is an import in all observed LAY files.

## LAYER Struct Layout (0x160 bytes)

Confirmed from FA.EXE decompilation of `FUN_004b4370`, `FUN_004b3be0`, `FUN_004b3cb0`, and `FUN_004b4720`.

| Offset | Type | Description |
|--------|------|-------------|
| +0x00 | u8 | **Flags** — bit 0: end-of-array sentinel; bit 1: brightness-gradient enabled |
| +0x0A | s32 | **alt_min** — altitude lower bound for this layer (inclusive) |
| +0x0E | s32 | **alt_max** — altitude upper bound for this layer (inclusive) |
| +0x22 | s32 | **extinction_param** — used by `GetLayerVisibility` to compute layer-range extinction value |
| +0x26 | s32 | **gradient_alt_start** — altitude at which brightness gradient begins |
| +0x2A | s32 | **gradient_val_start** — blend intensity at gradient_alt_start (0..0x100) |
| +0x2E | s32 | **gradient_alt_end** — altitude at which gradient reaches full intensity |
| +0x32 | s32 | **gradient_val_end** — blend intensity at gradient_alt_end (0..0x100) |
| +0x36 | u8[3] | **base_rgb** — 3-byte RGB colour used as blend target for brightness gradient and for nearest-match colour table lookup |
| +0x3A | ptr | **colour_entry_ptr** — pointer to matching entry in colour-matching table; set at load time by `FindNearestColorEntry` |
| +0x3E | u8[93] | **zenith_grad** — 31 × 3-byte RGB entries from zenith toward horizon (indices 0–30) |
| +0x9B | u8[96] | **horizon_grad** — 32 × 3-byte RGB entries from horizon downward (indices 0–31) |
| +0x102 | char[] | **cloud_pic** — ASCIIZ wildcard pattern for cloud PIC texture (e.g. `SKY*4.PIC`); loaded by `LoadPICByWildcard` |
| +0x118 | char[] | **sky_pic** — ASCIIZ wildcard pattern for secondary sky PIC texture |
| +0xFE | u32 | **fog_density** — per-frame fog opacity; updated each frame by `WRFogLayerUpdate` (random jitter ±25, clamped to [0xD9, 0xEB] = [217, 235]) |
| +0x14E | u8 | **visibility** — minimum visibility/opacity byte; consulted by layer-range queries |

The LAYER array is terminated when an entry's flag byte has bit 0 set. The array pointer is stored at offset 0x74 in the DLL data header (global `DAT_00580e24` in FA.EXE).

### Brightness gradient mechanics (`FUN_004b3cb0`)

When bit 1 of the flags byte is set, the engine dynamically tints the LAYER's gradient tables based on current altitude:

1. Factor = `(altitude − gradient_alt_start) × 0x100 / (gradient_alt_end − gradient_alt_start)`, clamped to `[0, 0x100]`.
2. **Horizon band** (`+0x9B..+0xFA`, 32 entries): each RGB triple is linearly blended toward `base_rgb` by `factor`. Full-intensity factor = fully tinted to base colour.
3. **Lower zenith band** (`+0x98..+0x6E` descending, 14 entries): same blend but factor decreases by `factor/15` per step — creates a smooth zenith-to-horizon brightness gradient.

Linear blend formula (used throughout): `dst = dst + (src − dst) × factor >> 8` (per-channel).

## Confirmed Functions

| Address | Name | Role |
|---------|------|------|
| `0x004b4370` | `ParseLayerFile` | Load LAY DLL, copy header to globals, init LAYER array |
| `0x004b46d0` | `FreeLayerFile` | Close/free the loaded LAY DLL handle |
| `0x004b3170` | `GetLayerByIndex` | `DAT_00583da8 = (&DAT_00580db0)[param_1]` — select active layer |
| `0x004b3750` | `CopyLayersToRuntime` | Copy LAYER entries from DLL data into `curLayers` array |
| `0x004b3820` | `InterpolateLayers` | Blend two LAYER structs based on altitude fraction |
| `0x004b3be0` | `GetLayerAtAltitude` | Search `curLayers` for entry spanning the given altitude; interpolate |
| `0x004b3ad0` | `FindNearestColorEntry` | Manhattan-distance nearest-colour match in the colour entry table |
| `0x004b3b60` | `LerpInt` | `*dst += (src − *dst) × factor >> 8` |
| `0x004b3b80` | `LerpRGB` | Per-channel lerp on 3-byte RGB entry |
| `0x004b3cb0` | `ApplyBrightnessGradient` | Altitude-driven tint of zenith/horizon RGB bands |
| `0x004b3d90` | `UpdateSkyState` | Per-frame: smooth-transition all atmosphere parameters, apply to working palette |
| `0x004b4170` | `UpdateAuroraClouds` | Aurora/cloud density update based on aircraft altitude and weather flags |
| `0x004b4680` | `LoadPICByWildcard` | Parse `*` wildcard range from LAYER string field, call `FUN_004c686c` to load PIC |
| `0x004b46f0` | `SetSkyActive` | `DAT_0050c8b8 = param_1` |
| `0x004b4700` | `ClearFrameColorTable` | Zero the 0xc0-dword frame colour buffer |
| `0x004b4720` | `GetLayerVisibility` | Walk LAYER entries in altitude range, return minimum visibility byte |
| `0x004b3190` | `GetLayerBoundary` | Search `curLayers` (base `DAT_00583250`) at stride 0x160 for entry spanning `alt >> 8`; sets `*param_2 = 1` if at a layer transition boundary |
| `0x004b4320` | `WRFogLayerUpdate` | Per-frame fog update: add random jitter in [−25, +26] to `LAYER+0xfe`, clamp to [0xD9, 0xEB] |
| `0x004aacf0` | `T_DefaultHorizon` | Default horizon renderer (FA.EXE); reads colour bytes from `DAT_00583da8+0xD4..+0xFC` (active LAYER colour table); calls `FUN_004c924c` / `FUN_004c942c` for gradient rendering |

## Location

| LIB | Count |
|-----|-------|
| FA_2.LIB | 24 |

## CODE Section Layout (Confirmed)

LAY files use **Phar Lap PE format** (signature `PL\0\0`). Unlike other small overlays (4608 bytes, CODE at file offset `0x200`), LAY files have a larger PE header structure:

| Section | VA | VSize | File offset | File size |
|---------|-----|-------|-------------|-----------|
| `CODE` | 0x1000 | 0x34D6 | **0x400** | 0x3600 |
| `.idata` | 0x5000 | 0x5C | 0x3A00 | 0x200 |
| `.reloc` | 0x6000 | 0x400 | 0x3C00 | 0x400 |
| `$$DOSX` | 0x7000 | 0x200 | 0x4000 | 0x200 |

(CLOUD1.LAY dimensions shown. DAY1.LAY CODE vsize = 0x40C6, file offset still 0x400.)

The CODE section contains all rendering data. The engine interprets this data; `_T_HorizonProc` (`0x4aace0` in FA.EXE, from FA.SMS) is the horizon renderer called by the LAY DLL.

### CODE section structure (VA offsets from 0x1000)

| VA range | Content |
|----------|---------|
| 0x1000–0x1077 | **DLL data header** — 0x78 bytes (0x1e dwords) copied verbatim to FA.EXE globals at load time. Contains: parameter fields, and VA pointers to the colour-matching table, palette buffer, and LAYER array (see header map below). |
| 0x1078–0x10AF | **Layer parameter sub-block** — VA back-pointer (0x1078), u32 count=38, INT_MAX sentinel (0x7FFFFFFF), u32 5000, u32 165, and additional u32 fields |
| 0x10B0–0x1175 | **Sky gradient / colour sub-block** — 8-byte header + 190 bytes of palette index data |
| 0x1176–0x11A5 | Zero-fill padding |
| 0x11A6–0x11FF | **Wave / scene parameter block** — u32 counts, VA pointer, wave amplitude/frequency bytes, `"wave1.SH"` string, INT_MAX sentinel, more VAs |
| 0x1200+ | **Second gradient block** + additional sub-tables; **LAYER array** at a VA specified in the header |

### DLL data header (VA 0x1000, first 0x78 bytes)

These 30 dwords are copied verbatim into FA.EXE's BSS segment at `DAT_00580db0` when the LAY file is loaded. After relocation all pointer fields hold absolute VAs within the loaded DLL image.

| Header offset | FA.EXE global | Role |
|--------------|---------------|------|
| +0x00–0x0F | DAT_00580db0–DAT_00580dbc | Parameter fields (counts/flags, exact semantics TBD) |
| +0x10 | DAT_00580dc0 | VA of default LAYER entry → copied to active-layer ptr (`DAT_00583da8`) at load |
| +0x14, +0x18, +0x1C | DAT_00580db4/db8/dbc | Parameters → `_DAT_0055be28/2c/30` |
| +0x2C | DAT_00580dcc | → `_DAT_0055be34` |
| +0x30, +0x34, +0x38, +0x3C, +0x40, +0x44 | DAT_00580dd0..de0 | → `_DAT_0055be38..48` |
| +0x58, +0x5C, +0x60, +0x64, +0x68 | DAT_00580df8..e08 | → `_DAT_0055be4c..5c` |
| +0x6C | DAT_00580e1c | VA of **colour entry table** (48-byte entries `[term][R][G][B][…]`) used by `FindNearestColorEntry` |
| +0x70 | DAT_00580e20 | VA of **palette buffer** (0xc0 × 4 bytes) — copied to working gradient buffer |
| +0x74 | DAT_00580e24 | VA of **LAYER array** (0x160-byte entries, terminated by entry[0] bit 0) |

### Identified sub-block types (from CODE inspection)

| Sub-block type | Example VA | Description |
|---------------|------------|-------------|
| **Identity table** | 0x3CF8 | 256-byte passthrough: `00 01 02 ... FF`. Used when no colour remapping is needed. |
| **Colour remap table** | 0x34F8 | 256-byte table mapping code indices to upper-palette entries (0xB4–0xBF = sky blue palette range in EGA/VGA extended palette). |
| **RGB triplet array** | 0x15F8 | 3-byte tuples in `(R, G, B)` form. Observed values: `00 3F 3F` = VGA full-brightness teal/cyan. |

### Sky gradient sub-block (0x10B0)

This sub-block sits between the DLL data header and the wave parameter block. It is **not** the LAYER array; it is one of the colour sub-tables referenced from within LAYER entries via the colour entry table.

```
31 00 00 00 00 00 10 10  ← 8-byte header
0E 3F 3F 3F 3B 3B 3C 38 38 39 35 35 36 31 31 32  ← gradient data begins
31 31 32 32 32 33 32 32 33 33 33 33 33 33 34 34
34 34 34 34 35 34 34 35 35 35 36 36 36 37 35 35
36 ...
```

**Header fields:**

| Offset | Type | Value | Meaning |
|--------|------|-------|---------|
| +0 | u32 | 0x31 = 49 | Entry count for this colour sub-table *(not fully confirmed — see TODO)* |
| +4 | u16 | 0 | Padding / reserved |
| +6 | u8 | 0x10 = 16 | Unknown (dimension / stride / channel count) |
| +7 | u8 | 0x10 = 16 | Unknown (dimension / stride / channel count) |

Gradient data (190 bytes, VA 0x10B8–0x1175) encodes sky colour as **VGA 6-bit palette indices** (range 0–63). Values are non-monotonic — not a simple linear ramp. The curve represents sky brightness/colour as a function of altitude band, e.g. bright at horizon (0x3F=63) transitioning through mid-sky blues (0x31–0x37) and darker values toward zenith. Multiple sequential sub-tables follow within the 190-byte range.

### Wave / scene parameter block (~0x11A6)

```
66 00 00 00  ← u32 0x66=102
DD 02 00 00  ← u32 0x2DD=733
00 00 00 00
D0 44 00 00  ← VA pointer (0x44D0, within CODE)
FE 1F 38 0E 70 62 00 00 30 0B 01 00 18 47 E8 B8  ← wave amplitude/freq params
4B 64 64 64 64                                   ← 5 bytes
77 61 76 65 31 2E 53 48 00                       ← "wave1.SH\0" — ocean mesh
FF FF FF 7F                                      ← INT_MAX sentinel
94 11 00 00  ← VA 0x1194
1C 25 00 00  ← VA 0x251C
...
```

`"wave1.SH"` is the ocean wave mesh loaded by the LAY DLL. The wave parameters (16 bytes preceding the string) are **identical across all CLOUD1 and DAY1 variants** — weather state does not affect wave motion physics.

### CLOUD1 vs DAY1 comparison

| Property | CLOUD1 | DAY1 |
|----------|--------|------|
| Total file | 16896 bytes | 20992 bytes (+4096) |
| CODE VSize | 0x34D6 (13526) | 0x40C6 (16582) |
| .reloc size | 0x400 (1024) | 0x800 (2048) |
| Differing bytes | — | 15202/16896 |

DAY1 has 3056 more bytes of CODE and 1024 more bytes of relocation data. The extra CODE contains additional sky gradient sub-tables and colour remap tables for clear-day conditions (no cloud cover). Nearly all bytes differ because inserting new sub-blocks shifts the VA space and invalidates most pointer values throughout the file.

### CLOUD1B is identical to CLOUD1

Binary diff of `CLOUD1B.LAY` vs `CLOUD1.LAY` shows **zero differences** — the files are byte-for-byte copies. The `B` suffix does **not** encode a brightness variant for this file. Its purpose (alias, duplicate, or reserved slot) is unknown.

## Toolkit Roadmap

LAYER struct layout and all major loading functions are now confirmed. Remaining before codec implementation:

- New `lib/src/lay.cpp` + `lib/include/ft/lay.h` — parse DLL data header, LAYER array, gradient byte arrays, wave params
- New `cli/cmd_lay.cpp`:
  - `ft lay dump <file.LAY>` — exports atmosphere parameters as JSON
  - `ft lay gradient <file.LAY> -o gradient.png` — renders sky colour ramp as a PNG strip (one pixel per step)

## TODO — Deep Dive

- Confirm `0x31` and `10 10` in gradient sub-block header: entry count vs. type ID; dimension/stride meaning. Trace the sub-block reader function that interprets these fields.
- Explain CLOUD1B = CLOUD1 (byte-for-byte identical — alias, stub, or reserved slot?)
- Map the `layer <name>.LAY <index>` slot index from `.MM` files to rendering layers
- Document CLOUD / DAY / other prefix naming convention
- Map the parameter fields at header offsets 0x00–0x0F and the gap at 0x1C–0x28 (not yet assigned to FA.EXE globals)

## Related

- [MM.md](MM.md) — theater files that reference `.LAY` files via the `layer` keyword
- [SH.md](SH.md) — `wave1.SH` is the ocean wave mesh loaded by `.LAY`
- [PIC.md](PIC.md) — `ocean*06.PIC` ocean texture atlas
