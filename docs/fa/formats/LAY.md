---
format: LAY
name: Sky and Atmosphere Layers
extensions: [".LAY"]
category: 3d
endianness: little
spec:
  status: partial
  gaps:
    - kind: re-static
      issue: 54
      note: "DLL data header field +0x00 semantics"
codec:
  direction: read
  issue: 99
  lib: [lib/src/lay.cpp, lib/src/pe.cpp]
  commands: [lay]
  tests: [tests/test_pe.cpp]
  fuzz: [fuzz/fuzz_pe.cpp]
  gui: [gui/src/editors/lay_editor.cpp]
  fixtures:
    synthetic: false
    real_manifest: true
related: [MM, SH, PIC]
---

# LAY — Sky and Atmosphere Layers (.LAY)

FA_2.LIB contains 24 `.LAY` files (e.g. `CLOUD1.LAY`, `DAY1.LAY`, `DAY2.LAY`).
Each defines a complete atmospheric rendering configuration — sky gradient,
cloud layers, horizon, and ocean surface — used during flight. Referenced by
name from `.MM` theater files (`layer day2.LAY 0`). Each is a **Win32 PE DLL**
loaded at runtime.

## Tools

### fx

```
fx lay dump     <file.LAY>                    # atmosphere parameters as JSON
fx lay gradient <file.LAY> -o gradient.png    # sky colour ramp as a PNG strip
```

## File Layout

All multi-byte integers are little-endian.

Win32 PE DLL (Phar Lap format, signature `PL\0\0`). `CLOUD1.LAY` decompresses
to **16896 bytes** — significantly larger than other overlays (4608 bytes),
because `.LAY` files embed the full sky/atmosphere rendering lookup tables as
data. `DAY1.LAY` decompresses to **20992 bytes**, confirming that file size is
not uniform across LAY variants.

String analysis, CODE section analysis, and the game executable decompilation reveal the
embedded content:

- **`wave1.SH`** — animated ocean wave mesh, referenced by name in both cloud
  and day variants (string at CODE VA ~0x11C2)
- **`_T_HorizonProc`** — **imported** from `main.dll` (= the game executable; one of the 2
  functions in the game executable's PE export table — see
  [architecture.md](../architecture.md#overlay-system--win32-pe-dlls)). LAY
  files call the engine's horizon rendering function rather than implementing
  it. (An earlier claim that `_T_HorizonProc` is an export was incorrect — it
  is an import in all observed LAY files.)
- **DLL data header** — first 0x78 bytes (0x1e dwords) of the CODE section are
  copied verbatim to the game executable globals at load time (full map below).
- **LAYER array** — `N` entries × 0x160 bytes each, at a CODE VA stored in the
  header. Each entry defines one sky condition (altitude range, gradient
  table, cloud textures, visibility).
- **Sky gradient tables** — byte arrays in CODE, values 0x09–0x3F (CLOUD1) or
  0x01–0x3C (DAY1). These are copied directly into LAYER entries at load time.
- **Wave parameter block** — 16 bytes at CODE VA ~0x11B0: fixed-point ocean
  wave amplitude/frequency parameters (identical between CLOUD1 and DAY1 — not
  weather-specific)
- **Colour palette** — 0xc0 (192) dwords at a VA in the header; copied to a
  working buffer used by the gradient blending pipeline.
- **Colour entry table** — 48-byte entries (stride 0x30); each entry
  `[terminator_byte][R][G][B][count:u32][colour_array:u32[]]`.
  `FindNearestColorEntry` walks entries (stops when `terminator_byte != 0`)
  computing Manhattan distance `|R_entry−R| + |G_entry−G| + |B_entry−B|` to
  find the nearest match for each LAYER's `base_rgb`. The match pointer is
  then stored in the LAYER's `colour_entry_ptr` (+0x3A). `colour_array`
  (starting at entry+8) contains `count` 4-byte palette indices used by
  `FUN_004b3410` to look up a rendering colour given a brightness factor.

### CODE Section Layout — confirmed

Unlike other small overlays (4608 bytes, CODE at file offset `0x200`), LAY
files have a larger PE header structure:

| Section | VA | VSize | File offset | File size |
|---------|-----|-------|-------------|-----------|
| `CODE` | 0x1000 | 0x34D6 | **0x400** | 0x3600 |
| `.idata` | 0x5000 | 0x5C | 0x3A00 | 0x200 |
| `.reloc` | 0x6000 | 0x400 | 0x3C00 | 0x400 |
| `$$DOSX` | 0x7000 | 0x200 | 0x4000 | 0x200 |

(CLOUD1.LAY dimensions shown. DAY1.LAY CODE vsize = 0x40C6, file offset still
0x400.)

The CODE section contains all rendering data. The engine interprets this data;
`_T_HorizonProc` (`0x4AACF0` in the game executable, confirmed from FA.SMS) is the horizon
renderer called by the LAY DLL.

### CODE section structure (VA offsets from 0x1000)

| VA range | Content |
|----------|---------|
| 0x1000–0x1077 | **DLL data header** — 0x78 bytes copied verbatim to the game executable globals at load time |
| 0x1078–0x10AF | **Layer parameter sub-block** — VA back-pointer (0x1078), u32 count=38, INT_MAX sentinel (0x7FFFFFFF), u32 5000, u32 165, and additional u32 fields |
| 0x10B0–0x1175 | **Sky gradient / colour sub-block** — 8-byte header + 190 bytes of palette index data |
| 0x1176–0x11A5 | Zero-fill padding |
| 0x11A6–0x11FF | **Wave / scene parameter block** — u32 counts, VA pointer, wave amplitude/frequency bytes, `"wave1.SH"` string, INT_MAX sentinel, more VAs |
| 0x1200+ | **Second gradient block** + additional sub-tables; **LAYER array** at a VA specified in the header |

### DLL data header (VA 0x1000, first 0x78 bytes)

These 30 dwords are copied verbatim into the game executable's BSS segment at `hdr` when the
LAY file is loaded. After relocation all pointer fields hold absolute VAs
within the loaded DLL image.

| Header offset | the game executable global | Role |
|--------------|---------------|------|
| `+0x00` | hdr | Parameter field (count/flags — semantics **unknown**, see Open Questions) |
| `+0x04` | DAT_00580db4 | → `_DAT_0055be28` |
| `+0x08` | DAT_00580db8 | → `_DAT_0055be2c` |
| `+0x0C` | DAT_00580dbc | → `_DAT_0055be30` |
| `+0x10` | DAT_00580dc0 | VA of default LAYER entry → copied to active-layer ptrs (`currentTintTable`, `currentShadeTable`, `DAT_005843c4`, `DAT_005843c8`) at load |
| `+0x14` | DAT_00580dc4 | **sky_angle_scale** — multiplied by above-horizon elevation angle, shifted >>8 to get `sky_layer_array` index; used by `SetActiveLayerByAngle` |
| `+0x18` | DAT_00580dc8 | **sky_layer_array[0]** — first element of 10-entry LAYER-ptr array indexed by above-horizon angle (`(&DAT_00580dc8)[idx]`); indices 1–6 also aliased to `_DAT_0055be34`..`_DAT_0055be48` by `ParseLayerFile`; indices 7–9 at +0x34–+0x3C (no named alias — accessed only via pointer arithmetic) |
| `+0x1C` | DAT_00580dcc | sky_layer_array[1] → `_DAT_0055be34` |
| `+0x20` | DAT_00580dd0 | sky_layer_array[2] → `_DAT_0055be38` |
| `+0x24` | DAT_00580dd4 | sky_layer_array[3] → `_DAT_0055be3c` |
| `+0x28` | DAT_00580dd8 | sky_layer_array[4] → `_DAT_0055be40` |
| `+0x2C` | DAT_00580ddc | sky_layer_array[5] → `_DAT_0055be44` |
| `+0x30` | DAT_00580de0 | sky_layer_array[6] → `_DAT_0055be48` |
| `+0x34` | DAT_00580de4 | **sky_layer_array[7]** — indices 7–9 are the array's high-angle tail; reachable when `(angle × sky_angle_scale) >> 8 ≥ 7`; no `_DAT_0055be*` alias assigned |
| `+0x38` | DAT_00580de8 | **sky_layer_array[8]** |
| `+0x3C` | DAT_00580dec | **sky_layer_array[9]** |
| `+0x40` | DAT_00580df0 | **below_angle_scale** — multiplied by `(−0xC0 − angle)` and shifted >>6 for below-horizon layer index; used by `SetActiveLayerByAngle` when `angle < −0xC0` |
| `+0x44` | DAT_00580df4 | **below_layer_array[0]** — first element of 10-entry LAYER-ptr array for below-horizon/underwater angles; indices 1–6 aliased to `_DAT_0055be4c`..`_DAT_0055be60`; indices 7–9 at +0x60–+0x68 |
| `+0x48` | DAT_00580df8 | below_layer_array[1] → `_DAT_0055be4c` |
| `+0x4C` | DAT_00580dfc | below_layer_array[2] → `_DAT_0055be50` |
| `+0x50` | DAT_00580e00 | below_layer_array[3] → `_DAT_0055be54` |
| `+0x54` | DAT_00580e04 | below_layer_array[4] → `_DAT_0055be58` |
| `+0x58` | DAT_00580e08 | below_layer_array[5] → `_DAT_0055be5c` |
| `+0x5C` | DAT_00580e0c | below_layer_array[6] → `_DAT_0055be60` |
| `+0x60` | DAT_00580e10 | **below_layer_array[7]** — high-angle tail; no `_DAT_0055be*` alias |
| `+0x64` | DAT_00580e14 | **below_layer_array[8]** |
| `+0x68` | DAT_00580e18 | **below_layer_array[9]** |
| `+0x6C` | DAT_00580e1c | VA of **colour entry table** (stride-0x30 entries: `[term][R][G][B][count u32][colour_array…]`) |
| `+0x70` | DAT_00580e20 | VA of **palette buffer** (0xc0 × 4 bytes) — copied to working gradient buffer |
| `+0x74` | DAT_00580e24 | VA of **LAYER array** (0x160-byte entries, terminated by entry[0] bit 0) |

### LAYER Struct Layout (0x160 bytes)

Confirmed from the game executable decompilation of `WRInit`, `FUN_004b3be0`,
`FUN_004b3cb0`, and `WRWeatherEffects`.

| Offset | Type | Description |
|--------|------|-------------|
| `+0x00` | u8 | **Flags** — bit 0: end-of-array sentinel; bit 1: brightness-gradient enabled |
| `+0x02` | s32 | **sel_alt_min** — **raw** altitude lower bound; `CopyLayersToRuntime` selects this layer when `sel_alt_min ≤ currentTimeOfDay ≤ sel_alt_max` |
| `+0x06` | s32 | **sel_alt_max** — raw altitude upper bound (same scale as `currentTimeOfDay`) |
| `+0x0A` | s32 | **alt_min** — rendering lower bound in `altitude >> 8` units; `GetLayerBoundary` compares `param_1 >> 8` against this field |
| `+0x0E` | s32 | **alt_max** — rendering upper bound in `altitude >> 8` units |
| `+0x12` | s32 | **fog_alt_low** — lower altitude bound of fog visibility ramp; below this `GetFogColour` returns `vis_lo` index |
| `+0x16` | s32 | **vis_lo** — fog visibility at `fog_alt_low` (0x00..0xFF; scaled to colour_array index as `vis_lo × count >> 8`) |
| `+0x1A` | s32 | **fog_alt_high** — upper altitude bound of fog visibility ramp; above this, `vis_hi` is used |
| `+0x1E` | s32 | **vis_hi** — fog visibility at `fog_alt_high`; between bounds the engine linearly interpolates |
| `+0x22` | s32 | **extinction_param** — used by `GetLayerVisibility` to compute layer-range extinction value |
| `+0x26` | s32 | **gradient_alt_start** — altitude at which brightness gradient begins |
| `+0x2A` | s32 | **gradient_val_start** — blend intensity at gradient_alt_start (0..0x100) |
| `+0x2E` | s32 | **gradient_alt_end** — altitude at which gradient reaches full intensity |
| `+0x32` | s32 | **gradient_val_end** — blend intensity at gradient_alt_end (0..0x100) |
| `+0x36` | u8[3] | **base_rgb** — 3-byte RGB colour used as blend target for brightness gradient and for nearest-match colour table lookup |
| `+0x3A` | ptr | **colour_entry_ptr** — pointer to matching entry in colour-matching table; set at load time by `FindNearestColorEntry` |
| `+0x3E` | u8[93] | **zenith_grad** — 31 × 3-byte RGB entries from zenith toward horizon (indices 0–30) |
| `+0x9B` | u8[96] | **horizon_grad** — 32 × 3-byte RGB entries from horizon downward (indices 0–31) |
| `+0xFB` | u8[3] | **horizon_base_rgb** — 3-byte RGB interpolated by `InterpolateLayers` (via `LerpRGB`) separately from `horizon_grad`. `T_DefaultHorizon` reads `+0xFC` (the Green byte) as the overcast/rain horizon base colour. |
| `+0xFE` | u32 | **fog_density** — per-frame fog opacity; updated each frame by `WRFogLayerUpdate` (random jitter ±25, clamped to [0xD9, 0xEB] = [217, 235]) |
| `+0x102` | char[] | **cloud_pic** — ASCIIZ wildcard pattern for cloud PIC texture (e.g. `SKY*4.PIC`); loaded by `LoadPICByWildcard` |
| `+0x118` | char[] | **sky_pic** — ASCIIZ wildcard pattern for secondary sky PIC texture |
| `+0x136` | ptr | **activate_fn** — function pointer called by `CopyLayersToRuntime` when this layer is selected; `NULL` for most layers |
| `+0x14E` | u8 | **visibility** — minimum visibility/opacity byte; consulted by layer-range queries |

The LAYER array is terminated when an entry's flag byte has bit 0 set. The
array pointer is stored at offset 0x74 in the DLL data header (global
`DAT_00580e24` in the game executable).

### Identified sub-block types (from CODE inspection)

| Sub-block type | Example VA | Description |
|---------------|------------|-------------|
| **Identity table** | 0x3CF8 | 256-byte passthrough: `00 01 02 ... FF`. Used when no colour remapping is needed. |
| **Colour remap table** | 0x34F8 | 256-byte table mapping code indices to upper-palette entries (0xB4–0xBF = sky blue palette range in EGA/VGA extended palette). |
| **RGB triplet array** | 0x15F8 | 3-byte tuples in `(R, G, B)` form. Observed values: `00 3F 3F` = VGA full-brightness teal/cyan. |

### Sky gradient sub-block (0x10B0)

This sub-block sits between the DLL data header and the wave parameter block.
It is **not** the LAYER array; it is one of the colour sub-tables referenced
from within LAYER entries via the colour entry table.

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
| `+0` | u32 | 0x31 = 49 | Builder metadata — not read by the game executable at runtime |
| `+4` | u16 | 0 | Builder metadata |
| `+6` | u8 | 0x10 = 16 | Builder metadata |
| `+7` | u8 | 0x10 = 16 | Builder metadata |

No the game executable function reads these four bytes. The engine accesses gradient and
colour data exclusively via pre-initialised pointers stored in LAYER struct
entries (e.g. `colour_entry_ptr` at +0x3A) and in the DLL data header; it never
decodes this 8-byte preamble. The header is a **LAY file builder artefact** —
present in all observed files but opaque to the runtime.

Gradient data (190 bytes, VA 0x10B8–0x1175) encodes sky colour as **VGA 6-bit
palette indices** (range 0–63). Values are non-monotonic — not a simple linear
ramp. The curve represents sky brightness/colour as a function of altitude
band, e.g. bright at horizon (0x3F=63) transitioning through mid-sky blues
(0x31–0x37) and darker values toward zenith. Multiple sequential sub-tables
follow within the 190-byte range.

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

`"wave1.SH"` is the ocean wave mesh loaded by the LAY DLL. The wave parameters
(16 bytes preceding the string) are **identical across all CLOUD1 and DAY1
variants** — weather state does not affect wave motion physics.

## File Inventory

All 24 files live in FA_2.LIB.

### Suffix naming convention — theater variants

Each base LAY name has up to five theater-specific variants, identified by a
single-letter suffix. The suffix assignment is confirmed by reading the `layer`
directive in every `.MM` theater file:

| Suffix | Theater | Example MM | LAY referenced |
|--------|---------|-----------|---------------|
| *(none)* | Ukraine, Kurile, Vietnam (Tonkin) | `UKR.MM`, `KURILE.MM`, `TVIET.MM` | `day2.LAY` |
| `B` | Baltic | `BAL.MM` | `day2b.LAY` |
| `E` | Egypt | `EGY.MM` | `day2e.LAY` |
| `F` | France | `FRA.MM` | `day2f.LAY` |
| `V` | Vladivostok | `VLA.MM` | `day2v.LAY` |
| `T` | *(unused — no MM file references `*t.LAY`)* | — | — |

The `T`-suffix files (`cloud1t.LAY`, `day2t.LAY`) are shipped in FA_2.LIB but
never loaded at runtime — no `.MM` file contains a `layer *t.LAY` directive.
They may be a residual from a cut theater or a tool-generated placeholder.

The `~*F.MM` suffix pattern (e.g. `~BALF.MM`, `~EGYF.MM`) is unrelated to the
LAY suffix — in `.MM` filenames `F` denotes the campaign **finale** mission
map, not France.

### CLOUD1 vs DAY1 comparison

| Property | CLOUD1 | DAY1 |
|----------|--------|------|
| Total file | 16896 bytes | 20992 bytes (+4096) |
| CODE VSize | 0x34D6 (13526) | 0x40C6 (16582) |
| .reloc size | 0x400 (1024) | 0x800 (2048) |
| Differing bytes | — | 15202/16896 |

DAY1 has 3056 more bytes of CODE and 1024 more bytes of relocation data. The
extra CODE contains additional sky gradient sub-tables and colour remap tables
for clear-day conditions (no cloud cover). Nearly all bytes differ because
inserting new sub-blocks shifts the VA space and invalidates most pointer
values throughout the file.

### CLOUD1B vs CLOUD1 — functionally identical, not byte-identical

Binary diff of `CLOUD1B.LAY` vs `CLOUD1.LAY` shows **3 bytes different**, all
at PE header offsets `0x0088`–`0x008A` (inside the Phar Lap PE header, before
the CODE section at file offset `0x400`). The gradient tables, LAYER array,
wave params, and all rendering data are identical. The 3-byte difference is a
PE-level field (likely checksum or build timestamp) updated by the LAY builder
tool when it generated the Baltic overcast variant. Visually, Baltic overcast
and Ukraine overcast are the same sky configuration — the difference is
metadata only.

## Engine Notes

### Brightness gradient mechanics (`FUN_004b3cb0`)

When bit 1 of the flags byte is set, the engine dynamically tints the LAYER's
gradient tables based on current altitude:

1. Factor = `(altitude − gradient_alt_start) × 0x100 / (gradient_alt_end −
   gradient_alt_start)`, clamped to `[0, 0x100]`.
2. **Horizon band** (`+0x9B..+0xFA`, 32 entries): each RGB triple is linearly
   blended toward `base_rgb` by `factor`. Full-intensity factor = fully tinted
   to base colour.
3. **Lower zenith band** (`+0x98..+0x6E` descending, 14 entries): same blend
   but factor decreases by `factor/15` per step — creates a smooth
   zenith-to-horizon brightness gradient.

Linear blend formula (used throughout):
`dst = dst + (src − dst) × factor >> 8` (per-channel).

### Fog colour mechanics (`FUN_004b3410` — `GetFogColour`)

Each LAYER defines a piecewise-linear altitude→visibility ramp:

1. If altitude ≤ `fog_alt_low` (+0x12): use `vis_lo` (+0x16) as the
   colour-array fraction.
2. If altitude ≥ `fog_alt_high` (+0x1A): use `vis_hi` (+0x1E).
3. Between: linearly interpolate `(vis_hi − vis_lo) × (alt − fog_alt_low) /
   (fog_alt_high − fog_alt_low) + vis_lo`.
4. Multiply result by `colour_entry.count`, shift >>8 to get the final
   colour-array index.
5. Return `colour_array[index]` from the entry pointed to by
   `colour_entry_ptr` (+0x3A).
6. Return value is a bool: `altitude ≤ extinction_param` (i.e., within the
   extinction/fog zone).

`WRSetRemaps` (`GetFogColourAtBoundary`) blends the colour from two adjacent
layers when near an altitude transition boundary, clamping the ramp fields
against per-frame weather globals before calling `GetFogColour`.

### Angle-based layer selection (`SetShadingTable` — `SetActiveLayerByAngle`)

The atmosphere changes depending on the view elevation angle (passed in AX
register as a signed 16-bit value):

- **Above horizon** (angle ≥ 1): index = `(angle × sky_angle_scale) >> 8`;
  select `sky_layer_array[index]` (header +0x18 base, up to 7 entries at
  +0x18..+0x30).
- **At or near horizon** (−0xC0 ≤ angle ≤ 0): use `sky_layer_array[0]`
  (header +0x18).
- **Below horizon** (angle < −0xC0, and `lighteningAllowed` flag set): index =
  `((−0xC0 − angle) × below_angle_scale) >> 6`; select
  `below_layer_array[index]` (header +0x44 base, entries at +0x44..+0x5C).

The result is written to `currentShadeTable` (secondary active-layer pointer
used by the rendering pipeline).

### Confirmed Functions

| Address | Name | Role |
|---------|------|------|
| `0x004b4370` | `ParseLayerFile` | Load LAY DLL, copy header to globals, init LAYER array |
| `0x004b46d0` | `FreeLayerFile` | Close/free the loaded LAY DLL handle |
| `0x004b3170` | `GetLayerByIndex` | `currentTintTable = (&hdr)[param_1]` — select active layer |
| `0x004b3750` | `CopyLayersToRuntime` | Copy LAYER entries from DLL data into `curLayers` array |
| `0x004b3820` | `InterpolateLayers` | Blend two LAYER structs based on altitude fraction |
| `0x004b3be0` | `GetLayerAtAltitude` | Search `curLayers` for entry spanning the given altitude; interpolate |
| `0x004b3ad0` | `FindNearestColorEntry` | Manhattan-distance nearest-colour match in the colour entry table |
| `0x004b3b60` | `LerpInt` | `*dst += (src − *dst) × factor >> 8` |
| `0x004b3b80` | `LerpRGB` | Per-channel lerp on 3-byte RGB entry |
| `0x004b3cb0` | `ApplyBrightnessGradient` | Altitude-driven tint of zenith/horizon RGB bands |
| `0x004b3d90` | `UpdateSkyState` | Per-frame: smooth-transition all atmosphere parameters, apply to working palette |
| `0x004b4170` | `UpdateAuroraClouds` | Aurora/cloud density update based on aircraft altitude and weather flags |
| `0x004b4680` | `LoadPICByWildcard` | Parse `*` wildcard range from LAYER string field, call `Sprintf` to load PIC |
| `0x004b46f0` | `SetSkyActive` | `DAT_0050c8b8 = param_1` |
| `0x004b4700` | `ClearFrameColorTable` | Zero the 0xc0-dword frame colour buffer |
| `0x004b4720` | `GetLayerVisibility` | Walk LAYER entries in altitude range, return minimum visibility byte |
| `0x004b3190` | `GetLayerBoundary` | Search `curLayers` (base `curLayers`) at stride 0x160 for entry spanning `alt >> 8`; sets `*param_2 = 1` if at a layer transition boundary |
| `0x004b3410` | `GetFogColour` | `(layer, altitude, *out_colour)` → bool; linearly interpolates `vis_lo..vis_hi` over `fog_alt_low..fog_alt_high`, looks up `colour_array[idx]` via `colour_entry_ptr`; returns `altitude ≤ extinction_param` |
| `0x004b31f0` | `GetFogColourAtBoundary` | Wrapper: calls `GetFogColour` for both the primary layer (`DAT_00580d90`) and the boundary layer found by `GetLayerBoundary`; blends output by altitude position within the transition zone |
| `0x004b4320` | `WRFogLayerUpdate` | Per-frame fog update: add random jitter in [−25, +26] to `LAYER+0xfe`, clamp to [0xD9, 0xEB] |
| `0x004b4790` | `ClearAtmosphereBuffer` | Clears 0x843 dwords at `DAT_00581140`; called at end of `ParseLayerFile` |
| `0x004cc4b4` | `SetActiveLayerByAngle` | Sets `currentShadeTable` (secondary active-layer ptr) from a signed 16-bit elevation angle: above horizon → `sky_layer_array[angle × sky_angle_scale >> 8]`; near/at horizon → `sky_layer_array[0]`; below −0xC0 → `below_layer_array[(−0xC0 − angle) × below_angle_scale >> 6]` |
| `0x004aacf0` | `T_DefaultHorizon` | Default horizon renderer (the game executable); reads colour bytes from `currentTintTable+0xD4..+0xFC` (active LAYER colour table); calls `SolidHorizon` / `GouraudHorizon` for gradient rendering |

## Open Questions

### 1. DLL data header field +0x00

The first dword of the data header is copied to the `hdr` global like every
other field, but no decompiled consumer reads it as anything more specific
than a count/flags parameter; its semantics are unknown.

*Status: open — re-static (#54)*

## Related

**Formats:** [MM](MM.md) — theater files that reference `.LAY` files via the
`layer` keyword; [SH](SH.md) — `wave1.SH` is the ocean wave mesh loaded by
`.LAY`; [PIC](PIC.md) — `ocean*06.PIC` ocean texture atlas and the
`cloud_pic`/`sky_pic` wildcard textures.

**Engine:** [architecture.md](../architecture.md#overlay-system--win32-pe-dlls)
— the overlay DLL loading architecture and the game executable's export table.
